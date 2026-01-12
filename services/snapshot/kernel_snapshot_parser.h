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

#ifndef KERNEL_SNAPSHOT_PARSER_H
#define KERNEL_SNAPSHOT_PARSER_H

#include <list>

#include "i_kernel_snapshot_parser.h"
#include "kernel_snapshot_trie.h"

namespace OHOS {
namespace HiviewDFX {

struct SnapshotCell {
    SnapshotSection sectionKey;
    const std::vector<std::string> &lines;
    size_t start;
    size_t end;
};

class KernelSnapshotParser : public IKernelSnapshotParser {
public:
    KernelSnapshotParser();
    std::vector<CrashMap> ParseAll(const std::string &cont) override;
private:
    void SplitByNewLine(const std::string &str, std::vector<std::string> &lines);
    std::vector<CrashMap> ParseSnapshot(std::vector<std::string> &snapshotLines);
    void ParseSameSeqSnapshot(const std::string& seqNum, const std::vector<std::string> &lines,
            std::vector<CrashMap>& crashMaps);
    CrashMap ParseSnapshotUnit(const std::vector<std::string> &lines, size_t &index);

    static bool PreProcessLine(std::string &line);
    static std::unordered_map<std::string, std::string> ConvertThreadInfoToPairs(const std::string &line);

    bool ProcessTransStart(const std::vector<std::string>& lines, size_t& index,
        std::string keyword, CrashMap& output);

    static void ParseTransStart(const SnapshotCell& cell, CrashMap &output);
    static void ParseThreadInfo(const SnapshotCell& cell, CrashMap &output);
    static void ParseStackBacktrace(const SnapshotCell& cell, CrashMap &output);
    static void ParseProcessRealName(const SnapshotCell& cell, CrashMap& output);
    static void ParseDefaultAction(const SnapshotCell& cell, CrashMap &output);

    void ProcessSnapshotSection(const SnapshotCell& cell, CrashMap &output);

    void InitializeParseTable();
    void InitializeKeywordTrie();
    using ParseFunction = void (*)(const SnapshotCell&, CrashMap&);
    std::unordered_map<SnapshotSection, ParseFunction> parseTable_;
    KernelSnapshotTrie snapshotTrie_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
