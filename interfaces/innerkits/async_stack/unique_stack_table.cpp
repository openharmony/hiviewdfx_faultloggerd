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
#include "unique_stack_table.h"

#include <sys/mman.h>
#include <sys/prctl.h>
#include <securec.h>
#include "dfx_log.h"
namespace OHOS {
namespace HiviewDFX {
UniqueStackTable* UniqueStackTable::Instance()
{
    static UniqueStackTable* instance = new UniqueStackTable();
    return instance;
}

bool UniqueStackTable::Init()
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    // index 0 for reserved
    if (tableBufMMap_ != nullptr) {
        return true;
    }
    availableIndex_ = 1;
    totalNodes_ = ((tableSize_ / sizeof(Node)) >> 1) << 1; // make it even.
    if (totalNodes_ > MAX_NODES_CNT) {
        DFXLOGW("Hashtable size limit, initial value too large!\n");
        return false;
    }

    availableNodes_ = totalNodes_;
    if (availableNodes_ == 0) {
        return false;
    }
    hashModulus_ = availableNodes_ - 1;
    hashStep_ = (totalNodes_ / (deconflictTimes_ * 2 + 1)); // 2 : double times
    auto retBufMMap = mmap(NULL, tableSize_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (retBufMMap == MAP_FAILED) {
        DFXLOGW("Failed to mmap!\n");
        return false;
    }
    tableBufMMap_ = retBufMMap;
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, tableBufMMap_, tableSize_, "async_stack_table");
    DFXLOGD(
        "Init totalNodes_: %{public}u, availableNodes_: %{public}u, availableIndex_: %{public}u \
        hashStep_: %{public}" PRIu64 ", hashModulus_: %{public}u",
        totalNodes_, availableNodes_, availableIndex_, hashStep_, hashModulus_);
    return true;
}

bool UniqueStackTable::Resize()
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (tableBufMMap_ == nullptr) {
        DFXLOGW("[%{public}d]: Hashtable not exist, fatal error!", __LINE__);
        return false;
    }

    uint32_t oldNumNodes = totalNodes_;
    DFXLOGW("Before resize, totalNodes_: %{public}u, availableNodes_: %{public}u, " \
        "availableIndex_: %{public}u  hashStep_: %{public}" PRIu64 "",
        totalNodes_, availableNodes_, availableIndex_, hashStep_);

    if ((totalNodes_ << RESIZE_MULTIPLE) > MAX_NODES_CNT) {
        DFXLOGW("Hashtable size limit, resize failed current cnt: %{public}u, max cnt: %{public}u",
            totalNodes_,
            MAX_NODES_CNT);
        return false;
    }

    uint32_t newtableSize = tableSize_ << RESIZE_MULTIPLE;
    void* newTableBuf = mmap(NULL, newtableSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (newTableBuf == MAP_FAILED) {
        return false;
    }
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, newTableBuf, newtableSize, "async_stack_table");
    if (memcpy_s(newTableBuf, newtableSize, tableBufMMap_, tableSize_) != 0) {
        DFXLOGE("Failed to memcpy table buffer");
    }
    munmap(tableBufMMap_, tableSize_);
    tableBufMMap_ = newTableBuf;
    tableSize_ = newtableSize;
    deconflictTimes_ += DECONFLICT_INCREASE_STEP;
    availableIndex_ += availableNodes_;
    totalNodes_ = ((newtableSize / sizeof(Node)) >> 1) << 1; // make it even.
    availableNodes_ = totalNodes_ - oldNumNodes;
    if (availableNodes_ == 0) {
        return false;
    }
    hashModulus_ = availableNodes_ - 1;
    hashStep_ = availableNodes_ / (deconflictTimes_ * 2 + 1); // 2: double times
    DFXLOGW("After resize, totalNodes_: %{public}u, availableNodes_: %{public}u, " \
        "availableIndex_: %{public}u hashStep_: %{public}" PRIu64 "",
        totalNodes_, availableNodes_, availableIndex_, hashStep_);
    return true;
}

uint64_t UniqueStackTable::PutPcInSlot(uint64_t thisPc, uint64_t prevIdx)
{
    Node *tableHead = reinterpret_cast<Node *>(tableBufMMap_);
    if (hashModulus_ == 0) {
        DFXLOGW("The value of the hashModulus_ is zero\n");
        return 0;
    }
    uint64_t curPcIdx = (((thisPc >> 2) ^ (prevIdx << 4)) % hashModulus_) + availableIndex_;
    uint8_t currentDeconflictTimes_ = deconflictTimes_;

    Node node;
    node.section.pc = thisPc;
    node.section.prevIdx = prevIdx;
    node.section.inKernel = !!(thisPc & PC_IN_KERNEL);
    while (currentDeconflictTimes_--) {
        Node* tableNode = (Node*)tableHead + curPcIdx;

        // empty case
        if (tableNode->value == 0) {
            tableNode->value = node.value;
            usedSlots_.emplace_back(uint32_t(curPcIdx));
            return curPcIdx;
        }
        // already inserted
        if (tableNode->value == node.value) {
            return curPcIdx;
        }

        curPcIdx += currentDeconflictTimes_ * hashStep_ + 1;
        if (availableNodes_ == 0) {
            return 0;
        }
        if (curPcIdx >= totalNodes_) {
            // make sure index 0 do not occupy
            curPcIdx -= (availableNodes_ - 1);
        }
    }

    DFXLOGW("Collison unresolved, need resize, usedSlots_.size(): %{public}zu, curPcIdx: %{public}" PRIu64 "",
        usedSlots_.size(), curPcIdx);
    return 0;
}

// todo add lock
uint64_t UniqueStackTable::PutPcsInTable(StackId *stackId, uintptr_t* pcs, size_t nr)
{
    if (!Init()) {
        DFXLOGW("init Hashtable failed, fatal error!");
        return 0;
    }
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    int64_t reverseIndex = static_cast<int64_t>(nr);
    uint64_t prev = 0;
    while (--reverseIndex >= 0) {
        uint64_t pc = pcs[reverseIndex];

        if (pc == 0) {
            continue;
        }
        prev = PutPcInSlot(pc, prev);
        if (prev == 0) {
            return 0;
        }
    }

    stackId->section.id = prev;
    stackId->section.nr = static_cast<uint64_t>(nr);
    return prev;
}

size_t UniqueStackTable::GetWriteSize()
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (tableBufMMap_ == nullptr) {
        DFXLOGW("[%{public}d]: Hashtable not exist, fatal error!", __LINE__);
        return 0;
    }
    size_t size = 0;
    size += sizeof(pid_);
    size += sizeof(tableSize_);
    uint32_t usedNodes = usedSlots_.size();
    size += sizeof(usedNodes);
    size += usedNodes * sizeof(uint32_t); // key index
    size += usedNodes * sizeof(uint64_t); // node value
    return size;
}

Node* UniqueStackTable::GetFrame(uint64_t stackId)
{
    Node *tableHead = reinterpret_cast<Node *>(tableBufMMap_);
    if (stackId >= totalNodes_) {
        // should not occur
        DFXLOGW("Failed to find frame by index: %{public}" PRIu64 "", stackId);
        return nullptr;
    }

    return (Node *)&tableHead[stackId];
}

bool UniqueStackTable::GetPcsByStackId(StackId stackId, std::vector<uintptr_t>& pcs)
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (tableBufMMap_ == nullptr) {
        DFXLOGW("Hashtable not exist, failed to find frame!");
        return false;
    }
    uint64_t nr = stackId.section.nr;
    uint64_t tailIdx = stackId.section.id;

    Node *node = GetFrame(tailIdx);
    while (nr-- && node != nullptr) {
        pcs.push_back(
            node->section.inKernel ? (node->section.pc | KERNEL_PREFIX) : node->section.pc);
        if (node->section.prevIdx == HEAD_NODE_INDEX) {
            break;
        }
        node = GetFrame(node->section.prevIdx);
    }
    return true;
}

bool UniqueStackTable::ImportNode(uint32_t index, const Node& node)
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    Node *tableHead = reinterpret_cast<Node *>(tableBufMMap_);
    if (index >= tableSize_) {
        return false;
    }
    tableHead[index].value = node.value;
    return true;
}
}
} // namespace OHOS
