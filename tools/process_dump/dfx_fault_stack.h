/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#ifndef DFX_FAULT_STACK_H
#define DFX_FAULT_STACK_H

#include <memory>
#include <vector>

#include "dfx_frame.h"
#include "dfx_regs.h"
#include "dfx_maps.h"

namespace OHOS {
namespace HiviewDFX {
struct MemoryBlockInfo {
    uintptr_t startAddr;
    uintptr_t nameAddr;
    uintptr_t size;
    std::vector<uintptr_t> content;
    std::string name;
};

// Only print first three frame stack
class FaultStack {
public:
    explicit FaultStack(int32_t tid) : tid_(tid) {};
    ~FaultStack() = default;

    void Print() const;
    void PrintRegisterMemory() const;
    bool CollectStackInfo(const std::vector<DfxFrame>& frames, bool needParseStack = false);
    bool CreateBlockForCorruptedStack(const std::vector<DfxFrame>& frames, uintptr_t prevEndAddr, uintptr_t size);
    void CollectRegistersBlock(std::shared_ptr<DfxRegs> regs, std::shared_ptr<DfxMaps> maps);
    bool ParseUnwindStack(std::shared_ptr<DfxMaps> maps, std::vector<DfxFrame>& frames);

private:
    bool ReadTargetMemory(uintptr_t addr, uintptr_t &value) const;
    uintptr_t AdjustAndCreateMemoryBlock(size_t index, uintptr_t prevSp, uintptr_t prevEndAddr, uintptr_t size);
    uintptr_t PrintMemoryBlock(const MemoryBlockInfo& info, uintptr_t stackStartAddr) const;
    MemoryBlockInfo CreateMemoryBlock(uintptr_t addr, uintptr_t offset, uintptr_t size, const std::string& name) const;

private:
    int32_t tid_;
    std::vector<MemoryBlockInfo> blocks_;
    std::vector<MemoryBlockInfo> registerBlocks_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
