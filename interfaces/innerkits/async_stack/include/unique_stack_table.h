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
#ifndef UNIQUE_STACK_TABLE_H
#define UNIQUE_STACK_TABLE_H

#include <cinttypes>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

#include <sys/mman.h>

namespace OHOS {
namespace HiviewDFX {
#define ADDR_BIT_LENGTH        40
#define IDX_BIT_LENGTH         23
#define KERNEL_FLAG_BIT_LENGTH 1
#define DECONFLICT_INCREASE_STEP  3
#define RESIZE_MULTIPLE          2
#define NR_BIT_LENGTH          41
constexpr uint32_t INITIAL_TABLE_SIZE = 1 * 1024 * 1024;
constexpr uint32_t MAX_NODES_CNT = 1 << IDX_BIT_LENGTH ;
constexpr uint64_t PC_IN_KERNEL = 1ull << 63;
constexpr uint64_t HEAD_NODE_INDEX = 0;
// FFFFFF0000000000
constexpr uint64_t KERNEL_PREFIX = 0xFFFFFFull << 40;
constexpr uint8_t INIT_DECONFLICT_ALLOWED = 22;

// align
#pragma pack(push, 4)

union Node {
    uint64_t value;
    struct {
        uint64_t pc : ADDR_BIT_LENGTH;
        uint64_t prevIdx : IDX_BIT_LENGTH;
        uint64_t inKernel : KERNEL_FLAG_BIT_LENGTH;
    } section;
};

struct UniStackNode {
    uint32_t index;
    Node node;
};

struct UniStackTableInfo {
    uint32_t pid;
    uint32_t tableSize;
    uint32_t numNodes;
    std::vector<UniStackNode> nodes;
};

union StackId {
    uint64_t value;
    struct {
        uint64_t id : IDX_BIT_LENGTH;
        uint64_t nr : NR_BIT_LENGTH;
    } section;
};

#pragma pack(pop)
static_assert(sizeof(Node) == 8, "Node size must be 8 byte");

class UniqueStackTable {
public:
    bool Init();
    static UniqueStackTable* Instance();

    UniqueStackTable() = default;

    explicit UniqueStackTable(pid_t pid) : pid_(pid)
    {
    }

    UniqueStackTable(pid_t pid, uint32_t size) : pid_(pid), tableSize_(size)
    {
    }

    UniqueStackTable(void* buf, uint32_t size, bool releaseBuffer = true)
        : tableBufMMap_(buf), tableSize_(size), releaseBuffer_(releaseBuffer)
    {
        totalNodes_ = ((tableSize_ / sizeof(Node)) >> 1) << 1;
    }

    ~UniqueStackTable()
    {
        if (tableBufMMap_ != nullptr && releaseBuffer_) {
            munmap(tableBufMMap_, tableSize_);
            tableBufMMap_ = nullptr;
        }
    }

    uint64_t PutPcsInTable(StackId *stackId, uintptr_t *pcs, size_t nr);
    bool GetPcsByStackId(const StackId stackId, std::vector<uintptr_t>& pcs);
    bool ImportNode(uint32_t index, const Node& node);
    size_t GetWriteSize();

    bool Resize();

    uint32_t GetPid()
    {
        return pid_;
    }

    uint32_t GetTabelSize()
    {
        return tableSize_;
    }

    std::vector<uint32_t>& GetUsedIndexes()
    {
        return usedSlots_;
    }

    Node* GetHeadNode()
    {
        return reinterpret_cast<Node *>(tableBufMMap_);
    }

private:
    Node* GetFrame(uint64_t stackId);
    uint64_t PutPcInSlot(uint64_t thisPc, uint64_t prevIdx);
    int32_t pid_ = 0;
    void* tableBufMMap_ = nullptr;
    uint32_t tableSize_ = INITIAL_TABLE_SIZE;
    std::vector<uint32_t> usedSlots_;
    uint32_t totalNodes_ = 0;
    // current available node count, include index 0
    uint32_t availableNodes_ = 0;
    uint32_t hashModulus_ = 0;
    // 0 for reserved, start from 1
    uint32_t availableIndex_ = 1;
    // for de-conflict
    uint64_t hashStep_ = 0;
    uint8_t deconflictTimes_ = INIT_DECONFLICT_ALLOWED;
    std::mutex stackTableMutex_;
    bool releaseBuffer_ = true;
};
}
}
#endif // HIEPRF_UNIQUE_STACK_TABLE_H