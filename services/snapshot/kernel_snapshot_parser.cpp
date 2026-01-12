/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "kernel_snapshot_parser.h"

#include "dfx_log.h"
#include "kernel_snapshot_kernel_frame.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr int SEQUENCE_LENGTH = 7;

CrashSection GetSnapshotMapCrashItem(SnapshotSection item)
{
    switch (item) {
        case SnapshotSection::DUMP_REGISTERS:
            return CrashSection::CREGISTERS;
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
}

KernelSnapshotParser::KernelSnapshotParser()
{
    InitializeParseTable();
    InitializeKeywordTrie();
}

bool KernelSnapshotParser::PreProcessLine(std::string& line)
{
    if (line.size() <= SEQUENCE_LENGTH || line[0] == '\t') {
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

std::unordered_map<std::string, std::string> KernelSnapshotParser::ConvertThreadInfoToPairs(const std::string& line)
{
    std::unordered_map<std::string, std::string> pairs;
    size_t pos = 0;
    while (pos < line.size()) {
        pos = line.find_first_not_of(' ', pos);
        if (pos == std::string::npos) {
            break;
        }
        size_t keyStart = pos;

        pos = line.find('=', pos);
        if (pos == std::string::npos) {
            break;
        }
        std::string key = line.substr(keyStart, pos - keyStart);

        size_t valueStart = ++pos;
        pos = line.find(',', pos);
        if (pos == std::string::npos) {
            pairs[key] = line.substr(valueStart);
            break;
        }
        pairs[key] = line.substr(valueStart, pos - valueStart);
        pos++;
    }
    return pairs;
}

void KernelSnapshotParser::ParseTransStart(const SnapshotCell& cell, CrashMap& output)
{
    /**
     * kernel crash snapshot transaction start format:
     * [AB_00][transaction start] now mono_time is [45.006871][1733329272.590140]
     */
    const std::string& cont = cell.lines[cell.start];
    if (cont.find("mono_time") == std::string::npos) {
        return;
    }
    auto msPos = cont.rfind(".");
    std::string millsecond;
    const int millsecondLen = 3;
    if (msPos != std::string::npos && msPos + 1 < cont.length()) {
        millsecond = cont.substr(msPos + 1, millsecondLen);
    }
    millsecond = millsecond.length() != millsecondLen ? "000" : millsecond;
    auto secondPos = cont.rfind("[");
    if (secondPos != std::string::npos && secondPos + 1 < cont.length()) {
        output[CrashSection::TIME_STAMP] = cont.substr(secondPos + 1, 10) + millsecond; // 10: second timestamp length
    }
}

void KernelSnapshotParser::ParseThreadInfo(const SnapshotCell& cell, CrashMap& output)
{
    if (cell.start + 1 > cell.end) {
        return;
    }
    /**
     * kernel crash snapshot thread info format:
     * Thread info:
     *   name=ei.hmsapp.music, tid=5601, state=RUNNING, sctime=40.362389062, tcb_cref=502520008108a, pid=5601,
     *   ppid=656, pgid=1, uid=20020048, cpu=7, cur_rq=7
     */
    std::string info = cell.lines[cell.start + 1];
    DFXLOGI("kernel snapshot thread info : %{public}s", info.c_str());
    auto pairs = ConvertThreadInfoToPairs(info);
    output[CrashSection::PROCESS_NAME] = pairs["name"]; // native process use this
    output[CrashSection::FAULT_THREAD_INFO] = "Tid:" + pairs["tid"] + ", Name: "  + pairs["name"] + "\n";
    output[CrashSection::PID] = pairs["pid"];
    output[CrashSection::UID] = pairs["uid"];
}

void KernelSnapshotParser::ParseStackBacktrace(const SnapshotCell& cell, CrashMap& output)
{
    KernelFrame frame;
    for (size_t i = cell.start + 1; i <= cell.end; i++) {
        frame.Parse(cell.lines[i]);
        size_t pos = i - cell.start - 1;
        output[CrashSection::FAULT_THREAD_INFO] += frame.ToString(pos) + "\n";
    }
}

void KernelSnapshotParser::ParseProcessRealName(const SnapshotCell& cell, CrashMap& output)
{
    /**
     * kernel crash snapshot process statistics format:
     * [ED_00]Process statistics:
     * [ED_00] name         tid  state   tcb_cref      sched_cnt cpu_cur rq_cur cls rtprio ni pri pid ppid pgid
     * [ED_00] SaInit1      1012 RUNNING 5022a0008106b 7         7       6       TS  -     0   20 799 1    1
     * [ED_00] audio_server 799  BLOCKED 5022a0008108a 325       4       6       TS  -     0   20 799 1    1
     */
    for (size_t i = cell.start + 2; i <= cell.end; i++) { // 2 : skip header
        const auto& item = cell.lines[i];
        size_t nameStart = item.find_first_not_of(' ');
        if (nameStart == std::string::npos) {
            continue;
        }
        size_t nameEnd = item.find_first_of(' ', nameStart);
        if (nameEnd == std::string::npos) {
            continue;
        }
        std::string name = item.substr(nameStart, nameEnd - nameStart);

        size_t tidStart = item.find_first_not_of(' ', nameEnd);
        if (tidStart == std::string::npos) {
            continue;
        }
        size_t tidEnd = item.find_first_of(' ', tidStart);
        if (tidEnd == std::string::npos) {
            continue;
        }
        std::string tid = item.substr(tidStart, tidEnd - tidStart);
        if (tid == output[CrashSection::PID]) {
            output[CrashSection::PROCESS_NAME] = name;
            break;
        }
    }
}

void KernelSnapshotParser::ParseDefaultAction(const SnapshotCell& cell, CrashMap& output)
{
    auto it = GetSnapshotMapCrashItem(cell.sectionKey);
    if (it == CrashSection::INVALID_SECTION) {
        return;
    }
    for (size_t i = cell.start + 1; i <= cell.end; i++) {
        output[it] += cell.lines[i] + "\n";
    }
}

void KernelSnapshotParser::InitializeParseTable()
{
    parseTable_ = {
        {SnapshotSection::TRANSACTION_START, KernelSnapshotParser::ParseTransStart},
        {SnapshotSection::THREAD_INFO, KernelSnapshotParser::ParseThreadInfo},
        {SnapshotSection::STACK_BACKTRACE, KernelSnapshotParser::ParseStackBacktrace},
        {SnapshotSection::PROCESS_STATISTICS, KernelSnapshotParser::ParseProcessRealName}
    };
}

void KernelSnapshotParser::InitializeKeywordTrie()
{
    for (const auto& item : SNAPSHOT_SECTION_KEYWORDS) {
        snapshotTrie_.Insert(item.key, item.type);
    }
}

void KernelSnapshotParser::ProcessSnapshotSection(const SnapshotCell& cell, CrashMap& output)
{
    auto it = parseTable_.find(cell.sectionKey);
    if (it != parseTable_.end() && it->second) {
        it->second(cell, output);
    } else {
        ParseDefaultAction(cell, output);
    }
}

bool KernelSnapshotParser::ProcessTransStart(const std::vector<std::string>& lines, size_t& index,
    std::string keyword, CrashMap& output)
{
    for (; index < lines.size(); index++) {
        if (lines[index].size() >= keyword.size() && std::equal(keyword.begin(), keyword.end(), lines[index].begin())) {
            break;
        }
    }

    if (index == lines.size()) {
        return false;
    }
    SnapshotCell cell {SnapshotSection::TRANSACTION_START, lines, index, index};
    ProcessSnapshotSection(cell, output);
    index++;
    return true;
}

CrashMap KernelSnapshotParser::ParseSnapshotUnit(const std::vector<std::string>& lines, size_t& index)
{
    CrashMap output;
    if (!ProcessTransStart(lines, index, SNAPSHOT_SECTION_KEYWORDS[0].key, output)) {
        return output;
    }

    // process other snapshot sections
    size_t curSectionLineNum = 0;
    SnapshotSection lastSectionKey = SnapshotSection::INVALID_SECTION;
    for (; index < lines.size(); index++) {
        SnapshotSection curSectionKey;
        if (!snapshotTrie_.MatchPrefix(lines[index], curSectionKey)) {
            continue;
        }
        if (curSectionLineNum == 0) {
            lastSectionKey = curSectionKey;
            curSectionLineNum = index;
            continue;
        }

        ProcessSnapshotSection({lastSectionKey, lines, curSectionLineNum, index - 1}, output);
        lastSectionKey = curSectionKey;
        curSectionLineNum = index;
        if (lastSectionKey == SnapshotSection::TRANSACTION_END) {
            break;
        }
    }
    return output;
}

void KernelSnapshotParser::ParseSameSeqSnapshot(const std::string& seqNum, const std::vector<std::string>& lines,
    std::vector<CrashMap>& crashMaps)
{
    size_t curLineNum = 0;

    while (curLineNum < lines.size()) {
        auto cm = ParseSnapshotUnit(lines, curLineNum);
        if (!cm.empty()) {
            cm[CrashSection::SEQ_NUM] = seqNum;
            crashMaps.emplace_back(cm);
        }
    }
}

std::vector<CrashMap> KernelSnapshotParser::ParseSnapshot(std::vector<std::string>& snapshotLines)
{
    std::unordered_map<std::string, std::vector<std::string>> kernelSnapshotMap;
    // devide snapshot info by sequence number
    for (auto &line : snapshotLines) {
        if (!PreProcessLine(line)) {
            continue;
        }

        std::string seqNum = line.substr(0, SEQUENCE_LENGTH);
        kernelSnapshotMap[seqNum].emplace_back(line.substr(SEQUENCE_LENGTH));
    }
    std::vector<CrashMap> crashMaps;

    for (auto &item : kernelSnapshotMap) {
        ParseSameSeqSnapshot(item.first, item.second, crashMaps);
    }
    return crashMaps;
}

void KernelSnapshotParser::SplitByNewLine(const std::string& str, std::vector<std::string>& lines)
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

std::vector<CrashMap> KernelSnapshotParser::ParseAll(const std::string& cont)
{
    std::vector<std::string> lines;
    SplitByNewLine(cont, lines);
    return ParseSnapshot(lines);
}
} // namespace HiviewDFX
} // namespace OHOS
