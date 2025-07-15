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

#ifndef KERNEL_SNAPSHOT_TRIE_H
#define KERNEL_SNAPSHOT_TRIE_H

#include "kernel_snapshot_data.h"
namespace OHOS {
namespace HiviewDFX {
class KernelSnapshotTrie {
public:
    bool Insert(const std::string& key, SnapshotSection type);
    bool MatchPrefix(const std::string& key, SnapshotSection& type) const;
    KernelSnapshotTrie();
    KernelSnapshotTrie(const KernelSnapshotTrie&) = delete;
    KernelSnapshotTrie& operator=(const KernelSnapshotTrie&) = delete;
private:
    struct TrieNode {
        bool isEnd = false;
        SnapshotSection sectionType;
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    };
    std::unique_ptr<TrieNode> root_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
