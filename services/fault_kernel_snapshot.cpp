/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fault_kernel_snapshot.h"

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <list>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#ifndef is_ohos_lite
#include "parameters.h"
#endif // !is_ohos_lite

#include "faultlogger_client_msg.h"

#include "dfx_log.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr const char * const KERNEL_KBOX_SNAPSHOT = "/sys/kbox/snapshot_clear";
constexpr const char * const KBOX_SNAPSHOT_DUMP_PATH = "/data/log/faultlog/temp/";
constexpr const char * const KERNEL_SNAPSHOT_CHECK_INTERVAL = "kernel_snapshot_check_interval";
constexpr const char * const DEFAULT_CHECK_INTERVAL = "60";
constexpr const char * const KERNEL_SNAPSHOT_REASON = "CppCrashKernelSnapshot";
constexpr int MIN_CHECK_INTERVAL = 3;
constexpr int BUFFER_LEN = 1024;
constexpr int SEQUENCE_LEN = 7;

enum class CrashSection {
    TIME_STAMP,
    PID,
    UID,
    PROCESS_NAME,
    REASON,
    FAULT_THREAD_INFO,
    REGISTERS,
    MEMORY_NEAR_REGISTERS,
    FAULT_STACK,
    MAPS,
    EXCEPTION_REGISTERS,
    INVALID_SECTION
};

enum class SnapshotSection {
    TRANSACTION_START,
    EXCEPTION_REGISTERS,
    ABORT_ADDRESS_PTE,
    THREAD_INFO,
    DUMP_REGISTERS,
    DUMP_FPU_OR_SIMD_REGISTERS,
    STACK_BACKTRACE,
    ELF_LOAD_INFO,
    DATA_ON_TARGET_OF_LAST_BL,
    DATA_AROUND_REGS,
    CONTENT_OF_USER_STACK,
    BASE_ACTV_DUMPED,
    TRANSACTION_END
};

struct SnapshotSectionInfo {
    SnapshotSection type;
    const char* key;
};

const SnapshotSectionInfo SNAPSHOT_SECTION_KEYWORDS[] = {
    {SnapshotSection::TRANSACTION_START,              "[transaction start] now mono_time"},
    {SnapshotSection::EXCEPTION_REGISTERS,            "Exception registers:"},
    {SnapshotSection::ABORT_ADDRESS_PTE,              "Abort address pte"},
    {SnapshotSection::THREAD_INFO,                    "Thread info:"},
    {SnapshotSection::DUMP_REGISTERS,                 "Dump registers:"},
    {SnapshotSection::DUMP_FPU_OR_SIMD_REGISTERS,     "Dump fpu or simd registers:"},
    {SnapshotSection::STACK_BACKTRACE,                "Stack backtrace"},
    {SnapshotSection::ELF_LOAD_INFO,                  "Elf load info"},
    {SnapshotSection::DATA_ON_TARGET_OF_LAST_BL,      "Data on target of last"},
    {SnapshotSection::DATA_AROUND_REGS,               "Data around regs"},
    {SnapshotSection::CONTENT_OF_USER_STACK,          "Contents of user stack"},
    {SnapshotSection::BASE_ACTV_DUMPED,               "[base actv dumped]"},
    {SnapshotSection::TRANSACTION_END,                "[transaction end] now mono_time"}
};

using CrashMap = std::unordered_map<CrashSection, std::string>;

void SaveSnapshot(CrashMap& output);

class CrashKernelFrame {
public:
    void Parse(const std::string& line)
    {
        size_t pos = 0;
        pc = ExtractContent(line, pos, '[', ']');
        fp = ExtractContent(line, ++pos, '[', ']');
        funcNameOffset = ExtractContent(line, ++pos, '<', '>');
        elf = ExtractContent(line, ++pos, '(', ')');
    }

    std::string ToString(int count) const
    {
        std::string data = std::string("#") + (count < 10 ? "0" : "") + std::to_string(count);
        data += " pc " + pc + " " + elf + " " + funcNameOffset;
        return data;
    }

private:
    std::string pc;
    std::string fp;
    std::string funcNameOffset;
    std::string elf;

    static std::string ExtractContent(const std::string& line, size_t& pos, char startChar, char endChar)
    {
        size_t start = line.find(startChar, pos);
        size_t end = line.find(endChar, start);
        pos = end + 1;
        if (start != std::string::npos && end != std::string::npos) {
            return line.substr(start + 1, end - start - 1);
        }
        return "";
    }
};

void ReportCrashEvent(CrashMap& output)
{
    if (output[CrashSection::UID].empty()) {
        DFXLOGE("uid is empty, not report");
        return;
    }

    void* handle = dlopen("libfaultlogger.z.so", RTLD_LAZY | RTLD_NODELETE);
    if (handle == nullptr) {
        DFXLOGW("Failed to dlopen libfaultlogger, %{public}s\n", dlerror());
        return;
    }

    auto addFaultLog = reinterpret_cast<void (*)(FaultDFXLOGIInner*)>(dlsym(handle, "AddFaultLog"));
    if (addFaultLog == nullptr) {
        DFXLOGW("Failed to dlsym AddFaultLog, %{public}s\n", dlerror());
        dlclose(handle);
        return;
    }

    FaultDFXLOGIInner info;
    const int base = 10;
    info.time = strtol(output[CrashSection::TIME_STAMP].c_str(), nullptr, base);
    info.id = static_cast<uint32_t>(strtoul(output[CrashSection::UID].c_str(), nullptr, base));
    info.pid = static_cast<int32_t>(strtol(output[CrashSection::PID].c_str(), nullptr, base));
    info.faultLogType = 2; // 2 : CPP_CRASH_TYPE
    info.module = output[CrashSection::PROCESS_NAME];
    info.reason = KERNEL_SNAPSHOT_REASON;
    info.summary = output[CrashSection::FAULT_THREAD_INFO];
    addFaultLog(&info);
    DFXLOGI("Finish report fault to FaultLogger (%{public}u,%{public}d)", info.id, info.pid);
    dlclose(handle);
}

std::string GetBuildInfo()
{
#ifndef is_ohos_lite
    static std::string buildInfo = OHOS::system::GetParameter("const.product.software.version", "Unknown");
    return buildInfo;
#else
    return "Unknown";
#endif
}

bool IsBetaVersion()
{
#ifndef is_ohos_lite
    return OHOS::system::GetParameter("const.logsystem.versiontype", "") == "beta";
#else
    return false;
#endif
}

bool PreProcessLine(std::string& line)
{
    if (line.size() <= SEQUENCE_LEN || line[0] == '\t') {
        return false;
    }
    // move timestamp to end
    if (isdigit(line[1])) {
        auto pos = line.find('[', 1);
        if (pos != std::string::npos) {
            std::string tmp = line.substr(0, pos);
            line = line.substr(pos) + tmp;
        }
    }
    return true;
}

std::unordered_map<std::string, std::string> ConvertThreadInfoToPairs(const std::string& line)
{
    std::unordered_map<std::string, std::string> pairs;
    size_t pos = 0;
    while (pos < line.size()) {
        while (pos < line.size() && line[pos] == ' ') {
            pos++;
        }
        size_t keyStart = pos;

        while (pos < line.size() && line[pos] != '=') {
            pos++;
        }
        if (pos >= line.size()) {
            break;
        }
        std::string key = line.substr(keyStart, pos - keyStart);

        size_t valueStart = ++pos;
        while (pos < line.size() && line[pos] != ',') {
            pos++;
        }
        if (pos >= line.size()) {
            break;
        }
        pairs[key] = line.substr(valueStart, pos - valueStart);
        pos++;
    }
    return pairs;
}

void ParseTransStart(const std::string& cont, CrashMap& output)
{
    if (cont.find("mono_time") == std::string::npos) {
        return;
    }
    auto pos = cont.rfind("[");
    if (pos != std::string::npos && pos + 1 < cont.length()) {
        output[CrashSection::TIME_STAMP] = cont.substr(pos + 1, 10) + "000"; // 10 : timestamp length
    }
}

void ParseThreadInfo(const std::vector<std::string>& lines, int start, int end, CrashMap& output)
{
    if (start + 1 > end) {
        return;
    }
    /**
     * kernel crash snapshot thread info format:
     * Thread info:
     *   name=ei.hmsapp.music, tid=5601, state=RUNNING, sctime=40.362389062, tcb_cref=502520008108a, pid=5601,
     *   ppid=656, pgid=1, uid=20020048, cpu=7, cur_rq=7
     */
    std::string info = lines[start + 1];
    DFXLOGI("kenel snapshot thread info : %{public}s", info.c_str());
    auto pairs = ConvertThreadInfoToPairs(info);
    output[CrashSection::PROCESS_NAME] = pairs["name"]; // native process use this
    output[CrashSection::FAULT_THREAD_INFO] = "Tid:" + pairs["tid"] + ", Name: "  + pairs["name"] + "\n";
    output[CrashSection::PID] = pairs["pid"];
    output[CrashSection::UID] = pairs["uid"];
}

void ParseStackBacktrace(const std::vector<std::string>& lines, int start, int end, CrashMap& output)
{
    CrashKernelFrame frame;
    for (int i = start + 1; i <= end; i++) {
        frame.Parse(lines[i]);
        output[CrashSection::FAULT_THREAD_INFO] += frame.ToString(i - start - 1) + "\n";
    }
}

CrashSection GetSnapshotMapCrashItem(const SnapshotSection& item)
{
    switch (item) {
        case SnapshotSection::DUMP_REGISTERS:
            return CrashSection::REGISTERS;
        case SnapshotSection::DATA_AROUND_REGS:
            return CrashSection::MEMORY_NEAR_REGISTERS;
        case SnapshotSection::CONTENT_OF_USER_STACK:
            return CrashSection::FAULT_STACK;
        case SnapshotSection::ELF_LOAD_INFO:
            return CrashSection::MAPS;
        case SnapshotSection::EXCEPTION_REGISTERS:
            return CrashSection::EXCEPTION_REGISTERS;
        default:
            return CrashSection::INVALID_SECTION;
    }
}

void ParseDefaultAction(const std::vector<std::string>& lines, int start, int end,
    SnapshotSection key, CrashMap& output)
{
    auto it = GetSnapshotMapCrashItem(key);
    if (it == CrashSection::INVALID_SECTION) {
        return;
    }
    for (int i = start + 1; i <= end; i++) {
        output[it] += lines[i] + "\n";
    }
}

void ProcessSnapshotSection(SnapshotSection sectionKey, const std::vector<std::string>& lines,
    size_t start, size_t end, CrashMap& output)
{
    switch (sectionKey) {
        case SnapshotSection::TRANSACTION_START:
            ParseTransStart(lines[start], output);
            break;
        case SnapshotSection::THREAD_INFO:
            ParseThreadInfo(lines, start, end, output);
            break;
        case SnapshotSection::STACK_BACKTRACE:
            ParseStackBacktrace(lines, start, end, output);
            break;
        default:
            ParseDefaultAction(lines, start, end, sectionKey, output);
            break;
    }
}

bool ProcessTransStart(const std::vector<std::string>& lines, size_t& index,
    std::list<std::pair<SnapshotSection, std::string>>& keywordList, CrashMap& output)
{
    const auto& keyword = keywordList.front().second;
    for (; index < lines.size(); index++) {
        if (StartsWith(lines[index], keyword)) {
            break;
        }
    }

    if (index == lines.size()) {
        return false;
    }

    ProcessSnapshotSection(SnapshotSection::TRANSACTION_START, lines, index, index, output);
    index++;
    keywordList.pop_front();
    return true;
}

void ParseSnapshotUnit(const std::vector<std::string>& lines, size_t& index)
{
    CrashMap output;
    std::list<std::pair<SnapshotSection, std::string>> keywordList;
    for (const auto& item : SNAPSHOT_SECTION_KEYWORDS) {
        keywordList.emplace_back(item.type, item.key);
    }

    if (!ProcessTransStart(lines, index, keywordList, output)) {
        return;
    }

    // process other snapshot sections
    int snapshotSecIndex = -1;
    SnapshotSection snapshotSecKey;
    bool isTransEnd = false;

    for (; index < lines.size() && !isTransEnd; index++) {
        for (auto it = keywordList.begin(); it != keywordList.end(); it++) {
            if (!StartsWith(lines[index], it->second)) {
                continue;
            }
            if (snapshotSecIndex == -1) {
                snapshotSecIndex = index;
                snapshotSecKey = it->first;
                break;
            }
            ProcessSnapshotSection(snapshotSecKey, lines, snapshotSecIndex, index - 1, output);
            snapshotSecIndex = index;
            snapshotSecKey = it->first;
            if (it->first == SnapshotSection::TRANSACTION_END) {
                isTransEnd = true;
            }
            keywordList.erase(it);
            break;
        }
    }

    SaveSnapshot(output);
    ReportCrashEvent(output);
}

void ParseSameSeqSnapshot(const std::vector<std::string>& lines)
{
    size_t curLineNum = 0;
    while (curLineNum < lines.size()) {
        ParseSnapshotUnit(lines, curLineNum);
    }
}

void ParseSnapshot(std::vector<std::string>& snapshotLines)
{
    std::unordered_map<std::string, std::vector<std::string>> kernelSnapshotMap;
    // devide snapshot info by sequence number
    for (auto &line : snapshotLines) {
        if (!PreProcessLine(line)) {
            continue;
        }

        std::string seqNum = line.substr(0, SEQUENCE_LEN);
        kernelSnapshotMap[seqNum].emplace_back(line.substr(SEQUENCE_LEN));
    }

    for (auto &item : kernelSnapshotMap) {
        ParseSameSeqSnapshot(item.second);
    }
}

std::string FilterEmptySection(const std::string& secHead, const std::string& secCont, const std::string& end)
{
    if (secCont.empty()) {
        return "";
    }
    return secHead + secCont + end;
}

void OutputToFile(const std::string& filePath, CrashMap& output)
{
    FILE* file = fopen(filePath.c_str(), "w");
    if (file == nullptr) {
        DFXLOGE("open file failed %{public}s errno %{public}d", filePath.c_str(), errno);
        return;
    }
    std::string outputCont;
    outputCont += FilterEmptySection("Build info: ", GetBuildInfo(), "\n");
    outputCont += FilterEmptySection("Timestamp: ", output[CrashSection::TIME_STAMP], "\n");
    outputCont += FilterEmptySection("Pid: ", output[CrashSection::PID], "\n");
    outputCont += FilterEmptySection("Uid: ", output[CrashSection::UID], "\n");
    outputCont += FilterEmptySection("Reason: ", KERNEL_SNAPSHOT_REASON, "\n");
    outputCont += FilterEmptySection("Exception registers:\n", output[CrashSection::EXCEPTION_REGISTERS], "");
    outputCont += FilterEmptySection("Fault thread info:\n", output[CrashSection::FAULT_THREAD_INFO], "");
    outputCont += FilterEmptySection("Registers:\n", output[CrashSection::REGISTERS], "");
    outputCont += FilterEmptySection("Memory near registers:\n", output[CrashSection::MEMORY_NEAR_REGISTERS], "");
    outputCont += FilterEmptySection("FaultStack:\n", output[CrashSection::FAULT_STACK], "");
    outputCont += FilterEmptySection("Elfs:\n", output[CrashSection::MAPS], "");
    if (fwrite(outputCont.c_str(), sizeof(char), outputCont.length(), file) != outputCont.length()) {
        DFXLOGE("write file failed %{public}s errno %{public}d", filePath.c_str(), errno);
    }
    if (fclose(file) != 0) {
        DFXLOGE("close file failed %{public}s errno %{public}d", filePath.c_str(), errno);
    }
}

void SaveSnapshot(CrashMap& output)
{
    if (output[CrashSection::PID].empty()) {
        DFXLOGE("pid is empty, not save snapshot");
        return;
    }

    std::string filePath = std::string(KBOX_SNAPSHOT_DUMP_PATH) + "cppcrash-" +
                           output[CrashSection::PID] + "-" +
                           output[CrashSection::TIME_STAMP];
    OutputToFile(filePath, output);
}

int GetSnapshotCheckInterval()
{
#ifndef is_ohos_lite
    std::string interval = OHOS::system::GetParameter(KERNEL_SNAPSHOT_CHECK_INTERVAL, DEFAULT_CHECK_INTERVAL);
#else
    std::string interval = DEFAULT_CHECK_INTERVAL;
#endif
    int value = static_cast<int>(strtol(interval.c_str(), nullptr, 10)); // 10 : decimal
    if (errno == ERANGE) {
        DFXLOGE("get snapshot check interval failed, use default interval");
        value = 60; // 60 : default interval
    }
    value = value < MIN_CHECK_INTERVAL ? MIN_CHECK_INTERVAL : value;

    DFXLOGI("monitor crash kernel snapshot interval %{public}d", value);
    return value;
}

void SplitByNewLine(const std::string& str, std::vector<std::string>& lines)
{
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find('\n', start);
        if (end == std::string::npos) {
            end = str.size();
        }
        lines.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
}

void MonitorCrashKernelSnapshot()
{
    DFXLOGI("enter %{public}s ", __func__);
    pthread_setname_np(pthread_self(), "KernelSnapshot");
    int interval = GetSnapshotCheckInterval();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        if (access(KERNEL_KBOX_SNAPSHOT, F_OK) < 0) {
            DFXLOGE("can't find %{public}s, just exit", KERNEL_KBOX_SNAPSHOT);
            return;
        }
        int snapshotFd = open(KERNEL_KBOX_SNAPSHOT, O_RDONLY);
        if (snapshotFd < 0) {
            DFXLOGE("open snapshot filed %{public}d", errno);
            continue;
        }

        char buffer[BUFFER_LEN] = {0};
        std::vector<std::string> snapshotLines;
        std::string snapshotCont;

        int ret = read(snapshotFd, buffer, BUFFER_LEN - 1);
        while (ret > 0) {
            snapshotCont += buffer;
            ret = read(snapshotFd, buffer, BUFFER_LEN - 1);
        }
        close(snapshotFd);

        SplitByNewLine(snapshotCont, snapshotLines);
        ParseSnapshot(snapshotLines);
    }
}
} // namespace

void FaultKernelSnapshot::StartMonitor()
{
    DFXLOGI("monitor kernel crash snapshot start!");
    if (!IsBetaVersion()) {
        DFXLOGW("monitor kernel crash snapshot func not support");
        return;
    }
    std::thread catchThread = std::thread([] { MonitorCrashKernelSnapshot(); });
    catchThread.detach();
}
} // namespace HiviewDFX
} // namespace OHOS
