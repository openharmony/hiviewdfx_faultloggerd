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

#include <utility>
#include <memory>
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
    if (stackTable_.tableBufMMap != nullptr) {
        return true;
    }
    stackTable_.availableIndex = 1;
    stackTable_.totalNodes = ((stackTable_.tableSize / sizeof(Node)) >> 1) << 1; // make it even.
    if (stackTable_.totalNodes > MAX_NODES_CNT) {
        DFXLOGW("Hashtable size limit, initial value too large!\n");
        return false;
    }

    stackTable_.availableNodes = stackTable_.totalNodes;
    if (stackTable_.availableNodes == 0) {
        return false;
    }
    stackTable_.hashModulus = stackTable_.availableNodes - 1;
    stackTable_.hashStep = (stackTable_.totalNodes / (deconflictTimes_ * 2 + 1)); // 2 : double times
    auto retBufMMap = mmap(NULL, stackTable_.tableSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (retBufMMap == MAP_FAILED) {
        DFXLOGW("Failed to mmap!\n");
        return false;
    }
    stackTable_.tableBufMMap = retBufMMap;
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, stackTable_.tableBufMMap, stackTable_.tableSize, "async_stack_table");
    DFXLOGD(
        "Init totalNodes_: %{public}u, availableNodes_: %{public}u, availableIndex_: %{public}u \
        hashStep_: %{public}" PRIu64 ", hashModulus_: %{public}u",
        stackTable_.totalNodes, stackTable_.availableNodes, stackTable_.availableIndex,
        stackTable_.hashStep, stackTable_.hashModulus);
    return true;
}

bool UniqueStackTable::SwitchExternalBuffer(void* buffer, size_t size)
{
    if (buffer == nullptr) {
        std::lock_guard<std::mutex> guard(stackTableMutex_);
        if (snapshot_) {
            stackTable_ = std::move(*snapshot_);
            snapshot_.reset();
            return true;
        }
        return false;
    }
    return InitWithExternalBuffer(buffer, size);
}

bool UniqueStackTable::InitWithExternalBuffer(void* buffer, size_t size)
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (snapshot_) {
        DFXLOGW("External buffer already initialized!\n");
        return false;
    }
    snapshot_ = std::make_unique<StackTable>();
    std::swap(*snapshot_, stackTable_);
    stackTable_.availableIndex = 1;
    stackTable_.totalNodes = ((size / sizeof(Node)) >> 1) << 1; // make it even.
    if (stackTable_.totalNodes > MAX_NODES_CNT) {
        DFXLOGW("Hashtable size limit, initial value too large!\n");
        snapshot_.reset();
        return false;
    }
    stackTable_.availableNodes = stackTable_.totalNodes;
    if (stackTable_.availableNodes == 0) {
        snapshot_.reset();
        return false;
    }
    stackTable_.hashModulus = stackTable_.availableNodes - 1;
    stackTable_.hashStep = (stackTable_.totalNodes / (deconflictTimes_ * 2 + 1)); // 2 : double times
    stackTable_.tableBufMMap = buffer;
    stackTable_.tableSize = size;
    DFXLOGI(
        "InitWithExternalBuffer totalNodes_: %{public}u, availableNodes_: %{public}u, availableIndex_: %{public}u \
        hashStep_: %{public}" PRIu64 ", hashModulus_: %{public}u",
        stackTable_.totalNodes, stackTable_.availableNodes, stackTable_.availableIndex,
        stackTable_.hashStep, stackTable_.hashModulus);
    return true;
}

bool UniqueStackTable::Resize()
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (stackTable_.tableBufMMap == nullptr) {
        DFXLOGW("[%{public}d]: Hashtable not exist, fatal error!", __LINE__);
        return false;
    }
    uint32_t oldNumNodes = stackTable_.totalNodes;
    DFXLOGW("Before resize, totalNodes_: %{public}u, availableNodes_: %{public}u, " \
        "availableIndex_: %{public}u  hashStep_: %{public}" PRIu64 "",
        stackTable_.totalNodes, stackTable_.availableNodes, stackTable_.availableIndex, stackTable_.hashStep);

    if ((stackTable_.totalNodes << RESIZE_MULTIPLE) > MAX_NODES_CNT) {
        DFXLOGW("Hashtable size limit, resize failed current cnt: %{public}u, max cnt: %{public}u",
            stackTable_.totalNodes,
            MAX_NODES_CNT);
        return false;
    }

    uint32_t newtableSize = stackTable_.tableSize << RESIZE_MULTIPLE;
    void* newTableBuf = mmap(NULL, newtableSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (newTableBuf == MAP_FAILED) {
        return false;
    }
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, newTableBuf, newtableSize, "async_stack_table");
    if (memcpy_s(newTableBuf, newtableSize, stackTable_.tableBufMMap, stackTable_.tableSize) != 0) {
        DFXLOGE("Failed to memcpy table buffer");
    }
    munmap(stackTable_.tableBufMMap, stackTable_.tableSize);
    stackTable_.tableBufMMap = newTableBuf;
    stackTable_.tableSize = newtableSize;
    deconflictTimes_ += DECONFLICT_INCREASE_STEP;
    stackTable_.availableIndex += stackTable_.availableNodes;
    stackTable_.totalNodes = ((newtableSize / sizeof(Node)) >> 1) << 1; // make it even.
    stackTable_.availableNodes = stackTable_.totalNodes - oldNumNodes;
    if (stackTable_.availableNodes == 0) {
        return false;
    }
    stackTable_.hashModulus = stackTable_.availableNodes - 1;
    stackTable_.hashStep = stackTable_.availableNodes / (deconflictTimes_ * 2 + 1); // 2: double times
    DFXLOGW("After resize, totalNodes_: %{public}u, availableNodes_: %{public}u, " \
        "availableIndex_: %{public}u hashStep_: %{public}" PRIu64 "",
        stackTable_.totalNodes, stackTable_.availableNodes, stackTable_.availableIndex, stackTable_.hashStep);
    return true;
}

uint64_t UniqueStackTable::PutPcInSlot(uint64_t thisPc, uint64_t prevIdx)
{
    Node *tableHead = reinterpret_cast<Node *>(stackTable_.tableBufMMap);
    if (stackTable_.hashModulus == 0) {
        DFXLOGW("The value of the hashModulus_ is zero\n");
        return 0;
    }
    uint64_t curPcIdx = (((thisPc >> 2) ^ (prevIdx << 4)) % stackTable_.hashModulus) + stackTable_.availableIndex;
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
            return curPcIdx;
        }
        // already inserted
        if (tableNode->value == node.value) {
            return curPcIdx;
        }

        curPcIdx += currentDeconflictTimes_ * stackTable_.hashStep + 1;
        if (stackTable_.availableNodes == 0) {
            return 0;
        }
        if (curPcIdx >= stackTable_.totalNodes) {
            // make sure index 0 do not occupy
            curPcIdx -= (stackTable_.availableNodes - 1);
        }
    }
    return 0;
}

uint64_t UniqueStackTable::PutPcsInTable(StackId *stackId, const uintptr_t* pcs, size_t nr)
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (stackTable_.tableBufMMap == nullptr) {
        return 0;
    }
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
    if (stackTable_.tableBufMMap == nullptr) {
        DFXLOGW("[%{public}d]: Hashtable not exist, fatal error!", __LINE__);
        return 0;
    }
    size_t size = 0;
    size += sizeof(pid_);
    size += sizeof(stackTable_.tableSize);
    uint32_t usedNodes = stackTable_.usedSlots.size();
    size += sizeof(usedNodes);
    size += usedNodes * sizeof(uint32_t); // key index
    size += usedNodes * sizeof(uint64_t); // node value
    return size;
}

Node* UniqueStackTable::GetFrame(uint64_t stackId)
{
    Node *tableHead = reinterpret_cast<Node *>(stackTable_.tableBufMMap);
    if (stackId >= stackTable_.totalNodes) {
        // should not occur
        DFXLOGW("Failed to find frame by index: %{public}" PRIu64 "", stackId);
        return nullptr;
    }

    return (Node *)&tableHead[stackId];
}

bool UniqueStackTable::GetPcsByStackId(StackId stackId, std::vector<uintptr_t>& pcs)
{
    std::lock_guard<std::mutex> guard(stackTableMutex_);
    if (stackTable_.tableBufMMap == nullptr) {
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
    Node *tableHead = reinterpret_cast<Node *>(stackTable_.tableBufMMap);
    if (index >= stackTable_.tableSize) {
        return false;
    }
    tableHead[index].value = node.value;
    return true;
}
}
} // namespace OHOS
