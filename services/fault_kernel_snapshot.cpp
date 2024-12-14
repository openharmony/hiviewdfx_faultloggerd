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
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <thread>
#include <unistd.h>

#ifndef is_ohos_lite
#include "parameters.h"
#endif // !is_ohos_lite

#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr const char * const KBOX_SNAPSHOT_CLEAR = "/sys/kbox/snapshot_clear";
constexpr const char * const KBOX_SNAPSHOT_DUMP_PATH = "/data/log/faultlog/temp/";
constexpr const char * const KERNEL_SNAPSHOT_CHECK_INTERVAL = "kernel_snapshot_check_interval";
constexpr const char * const DEFAULT_CHECK_INTERVAL = "60";
constexpr int MIN_CHECK_INTERVAL = 3;
constexpr int BUFFER_LEN = 1024;
constexpr int SEQUENCE_LEN = 7;

enum class CrashSection {
    BUILD_INFO,
    TIME_STAMP,
    PID,
    UID,
    PROCESS_LIFE_TIME,
    REASON,
    FAULT_THREAD_INFO,
    REGISTERS,
    OTHER_THREAD_INFO,
    MEMORY_NEAR_REGISTERS,
    FAULT_STACK,
    MAPS,
    EXCEPTION_REGISTERS,
};

using CrashMap = std::map<CrashSection, std::string>;

void CreateOutputFile(CrashMap &output);
void MonitorCrashKernelSnapshot();
void ParseSections(std::vector<std::string> &lines);

class CrashKernelFrame {
public:
    void Parse(const std::string &line)
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

    static std::string ExtractContent(const std::string &line, size_t &pos, char startChar, char endChar)
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

std::string GetBuildInfo()
{
#ifndef is_ohos_lite
    static std::string buildInfo = OHOS::system::GetParameter("const.product.software.version", "Unknown");
#else
    static std::string buildInfo = "Unknown";
#endif
    return buildInfo;
}

std::string GetIntervalByParam()
{
#ifndef is_ohos_lite
    static std::string interval = OHOS::system::GetParameter(KERNEL_SNAPSHOT_CHECK_INTERVAL, DEFAULT_CHECK_INTERVAL);
#else
    static std::string interval = "Unknown";
#endif
    return interval;
}

bool IsDevMode()
{
#ifndef is_ohos_lite
    std::string isDev = OHOS::system::GetParameter("const.security.developermode.state", "");
#else
    std::string isDev = "";
#endif
    return isDev == "true";
}

bool ProcessLine(std::string &line)
{
    if (line.size() <= SEQUENCE_LEN || line[0] == '\t') {
        return false;
    }
    if (line[1] <= '9') { // 9 : if timestamp, move to end
        size_t i = 1;
        for (; i < line.size(); i++) {
            if (line[i] == '[') {
                break;
            }
        }
        std::string tmp = line.substr(0, i);
        line = line.substr(i) + tmp;
    }
    return true;
}

void ParseKernelCrashInfo(std::vector<std::string> &kernelSnapshotLines)
{
    std::map<std::string, std::vector<std::string>> kernelSnapshotInfoMap;
    // devide by sequence number
    for (auto &line : kernelSnapshotLines) {
        if (!ProcessLine(line)) {
            continue;
        }

        std::string sequenceNumber = line.substr(0, SEQUENCE_LEN);
        kernelSnapshotInfoMap[sequenceNumber].emplace_back(line.substr(SEQUENCE_LEN));
    }

    for (auto &item : kernelSnapshotInfoMap) {
        ParseSections(item.second);
    }
}

void ExtractKeyValuePair(std::string &line, size_t &curIndex, std::string &output)
{
    while (curIndex < line.size() && line[curIndex] != '=') {
        curIndex++;
    }
    size_t startPos = curIndex;
    while (line[curIndex] != ',' && curIndex < line.size()) {
        curIndex++;
    }
    if (startPos + 1 > line.size()) {
        return;
    }
    output = line.substr(startPos + 1, curIndex - startPos - 1);
}

void ParseTransStart(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    std::string cont = lines[start];
    if (cont.find("mono_time") != std::string::npos) {
        auto pos = cont.rfind("[");
        if (pos != std::string::npos && pos + 1 < cont.length()) {
            output[CrashSection::TIME_STAMP] = cont.substr(pos + 1, 10); // 10 : timestamp length
        }
    }
}

void ParseThreadInfo(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    if (start + 1 > end) {
        return;
    }
    std::string info = lines[start + 1];
    DFXLOGI("kenel snapshot thread info : %{public}s", info.c_str());
    size_t curIndex = 0;
    std::string tmpValue;
    std::string threadName;
    ExtractKeyValuePair(info, curIndex, threadName);
    ExtractKeyValuePair(info, curIndex, tmpValue);
    output[CrashSection::FAULT_THREAD_INFO] += std::string("Tid:") + tmpValue + ", Name: "  + threadName + "\n";
    ExtractKeyValuePair(info, curIndex, tmpValue);
    ExtractKeyValuePair(info, curIndex, tmpValue);
    ExtractKeyValuePair(info, curIndex, tmpValue);
    ExtractKeyValuePair(info, curIndex, output[CrashSection::PID]);
}

void ParseStackBacktrace(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    CrashKernelFrame frame;
    for (int i = start + 1; i <= end; i++) {
        frame.Parse(lines[i]);
        output[CrashSection::FAULT_THREAD_INFO] += frame.ToString(i - start - 1) + "\n";
    }
}

void ParseRegisters(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    for (int i = start + 1; i <= end; i++) {
        output[CrashSection::REGISTERS] += lines[i] + "\n";
    }
}

void ParseElfInfo(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    for (int i = start + 1; i <= end; i++) {
        output[CrashSection::MAPS] += lines[i] + "\n";
    }
}

void ParseUserStack(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    for (int i = start + 1; i <= end; i++) {
        output[CrashSection::FAULT_STACK] += lines[i] + "\n";
    }
}

void ParseDataAroundRegs(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    for (int i = start + 1; i <= end; i++) {
        output[CrashSection::MEMORY_NEAR_REGISTERS] += lines[i] + "\n";
    }
}

void ParseExceptionRegisters(std::vector<std::string> &lines, int start, int end, CrashMap &output)
{
    for (int i = start + 1; i <= end; i++) {
        output[CrashSection::EXCEPTION_REGISTERS] += lines[i] + "\n";
    }
}

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

std::list<std::pair<SnapshotSection, std::string>> g_sectionKeywords = {
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

using ParserFunction = std::function<void(std::vector<std::string> &, size_t &, size_t &, CrashMap &)>;

bool StartWith(std::string &str, std::string &prefix)
{
    size_t index = 0;
    while (str[index] == ' ') {
        index++;
    }
    return str.size() >= prefix.size() && str.compare(index, prefix.size(), prefix) == 0;
}

void ProcessSection(SnapshotSection sectionHeadKey, std::vector<std::string> &lines, size_t start, size_t end,
    CrashMap &output)
{
    static std::map<SnapshotSection, ParserFunction> secParserFuncMap = {
        {SnapshotSection::TRANSACTION_START, ParseTransStart},
        {SnapshotSection::EXCEPTION_REGISTERS, ParseExceptionRegisters},
        {SnapshotSection::THREAD_INFO, ParseThreadInfo},
        {SnapshotSection::DUMP_REGISTERS, ParseRegisters},
        {SnapshotSection::STACK_BACKTRACE, ParseStackBacktrace},
        {SnapshotSection::DATA_AROUND_REGS, ParseDataAroundRegs},
        {SnapshotSection::CONTENT_OF_USER_STACK, ParseUserStack},
        {SnapshotSection::ELF_LOAD_INFO, ParseElfInfo},
    };

    auto it = secParserFuncMap.find(sectionHeadKey);
    if (it != secParserFuncMap.end()) {
        it->second(lines, start, end, output);
    }
}

void ParseSingleCrash(std::vector<std::string> &lines, size_t &index)
{
    CrashMap output;
    auto tmpKeywordList = g_sectionKeywords;

    for (; index < lines.size(); index++) {
        if (StartWith(lines[index], tmpKeywordList.front().second)) {
            break;
        }
    }

    if (index == lines.size()) {
        return;
    }

    ProcessSection(SnapshotSection::TRANSACTION_START, lines, index, index, output);
    index++;
    tmpKeywordList.pop_front();

    // find new crash sections
    int secLine = -1;
    auto secHeadKey = SnapshotSection::TRANSACTION_START;
    bool isTransEnd = false;

    for (; index < lines.size() && !isTransEnd; index++) {
        for (auto it = tmpKeywordList.begin(); it != tmpKeywordList.end(); it++) {
            if (!StartWith(lines[index], it->second)) {
                continue;
            }
            if (secLine == -1) {
                secLine = index;
                secHeadKey = it->first;
                break;
            }
            ProcessSection(secHeadKey, lines, secLine, index - 1, output);
            secLine = index;
            secHeadKey = it->first;
            tmpKeywordList.erase(it);
            if (it->first == SnapshotSection::TRANSACTION_END) {
                isTransEnd = true;
            }
            break;
        }
    }

    CreateOutputFile(output);
}

void ParseSections(std::vector<std::string> &lines)
{
    size_t index = 0;
    while (index < lines.size()) {
        ParseSingleCrash(lines, index);
    }
}

void FormatExceptionCrash(std::ofstream &file, CrashMap &output)
{
    file << "Build info: " << output[CrashSection::BUILD_INFO] << std::endl;
    file << "Timestamp: " << output[CrashSection::TIME_STAMP] << std::endl;
    file << "Pid: " << output[CrashSection::PID] << std::endl;
    if (!output[CrashSection::EXCEPTION_REGISTERS].empty()) {
        file << "Exception registers:\n" << output[CrashSection::EXCEPTION_REGISTERS];
    }
    file << "Fault thread info:\n" << output[CrashSection::FAULT_THREAD_INFO];
    file << "Registers:\n" << output[CrashSection::REGISTERS];
    if (!output[CrashSection::MEMORY_NEAR_REGISTERS].empty()) {
        file << "Memory near registers:\n" << output[CrashSection::MEMORY_NEAR_REGISTERS];
    }
    if (!output[CrashSection::FAULT_STACK].empty()) {
        file << "FaultStack:\n" << output[CrashSection::FAULT_STACK];
    }
    file << "Elfs:\n" << output[CrashSection::MAPS];
}

void CreateOutputFile(CrashMap &output)
{
    if (output[CrashSection::PID].empty()) {
        return;
    }

    std::string fileName = std::string(KBOX_SNAPSHOT_DUMP_PATH) + "snapshot-" +
                           output[CrashSection::PID] + "-" +
                           output[CrashSection::TIME_STAMP] + "000";
    std::ofstream file(fileName, std::ios::out | std::ios::trunc);
    output[CrashSection::BUILD_INFO]  = GetBuildInfo();
    if (file.is_open()) {
        FormatExceptionCrash(file, output);
        file.close();
    } else {
        DFXLOGE("open file failed %{public}s errno %{public}d", fileName.c_str(), errno);
    }
}

int GetKernelSnapshotInterval()
{
    std::string interval = GetIntervalByParam();
    char *endPtr = nullptr;
    int value = static_cast<int>(strtol(interval.c_str(), &endPtr, 10));
    if (*endPtr != '\0' && interval.c_str() == endPtr) {
        DFXLOGE("Failed to cast unexpecterd_die_catch val to int %{public}d", errno);
        value = 60; // 60 : 1 min
    } else {
        value = value < MIN_CHECK_INTERVAL ? MIN_CHECK_INTERVAL : value;
    }
    DFXLOGI("monitor crash kernel snapshot interval %{public}d", value);
    return value;
}

void FillKernelSnapshot(char *buf, int len, std::vector<std::string> &kernelSnapshotLines, std::string &endString)
{
    int start = 0;
    std::string line(buf);
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            if (start == 0 && !endString.empty()) {
                kernelSnapshotLines.emplace_back(endString + line.substr(start, i - start));
                endString.clear();
            } else {
                kernelSnapshotLines.emplace_back(line.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    if (start < len) {
        endString = line.substr(start, len - start);
    }
}

void MonitorCrashKernelSnapshot()
{
    DFXLOGI("enter %{public}s ", __func__);
    int interval = GetKernelSnapshotInterval();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        if (access(KBOX_SNAPSHOT_CLEAR, F_OK) < 0) {
            DFXLOGE("can't find %{public}s, just exit", KBOX_SNAPSHOT_CLEAR);
            return;
        }
        int snapshotFd = open(KBOX_SNAPSHOT_CLEAR, O_RDONLY);
        if (snapshotFd < 0) {
            DFXLOGE("open snapshot filed %{public}d", errno);
            continue;
        }
        char buffer[BUFFER_LEN] = {0};
        std::vector<std::string> kernelSnapshotLines;
        int ret = read(snapshotFd, buffer, BUFFER_LEN - 1);
        std::string endString = "";

        while (ret > 0) {
            FillKernelSnapshot(buffer, BUFFER_LEN - 1, kernelSnapshotLines, endString);
            ret = read(snapshotFd, buffer, BUFFER_LEN - 1);
        }
        close(snapshotFd);
        ParseKernelCrashInfo(kernelSnapshotLines);
    }
}
} // namespace

void FaultKernelSnapshot::StartMonitor()
{
    DFXLOGI("monitor kernel crash snapshot start!");
    if (!IsDevMode()) {
        DFXLOGW("monitor kernel crash snapshot func not support");
        return;
    }
    std::thread catchThread = std::thread([] { MonitorCrashKernelSnapshot(); });
    catchThread.detach();
}
} // namespace HiviewDFX
} // namespace OHOS
