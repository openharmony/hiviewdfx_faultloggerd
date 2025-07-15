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

#include "kernel_snapshot_trie.h"

namespace OHOS {
namespace HiviewDFX {

KernelSnapshotTrie::KernelSnapshotTrie() : root_(new TrieNode())
{
}

bool KernelSnapshotTrie::Insert(const std::string& key, SnapshotSection type)
{
    if (key.empty()) {
        return false;
    }

    TrieNode* currentNode = root_.get();
    for (char ch : key) {
        if (currentNode->children.find(ch) == currentNode->children.end()) {
            currentNode->children[ch] = std::unique_ptr<TrieNode>(new TrieNode());
        }
        currentNode = currentNode->children[ch].get();
    }

    currentNode->isEnd = true;
    currentNode->sectionType = type;
    return true;
}

bool KernelSnapshotTrie::MatchPrefix(const std::string& key, SnapshotSection& type) const
{
    TrieNode* currentNode = root_.get();
    for (char ch : key) {
        auto it = currentNode->children.find(ch);
        if (it == currentNode->children.end()) {
            return false;
        }
        currentNode = it->second.get();
        if (currentNode->isEnd) {
            type = currentNode->sectionType;
            return true;
        }
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
