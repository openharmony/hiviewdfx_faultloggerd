/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

    bool CollectStackInfo(std::shared_ptr<DfxRegs> reg, const std::vector<std::shared_ptr<DfxFrame>> &frames);
    void Print();

private:
    bool ReadTargetMemory(uintptr_t addr, uintptr_t &value);
    uintptr_t AdjustAndCreateMemoryBlock(size_t index, uintptr_t prevSp, uintptr_t prevEndAddr, uintptr_t size);
    uintptr_t PrintMemoryBlock(const MemoryBlockInfo& info);
    void CreateMemoryBlock(uintptr_t addr, uintptr_t offset, uintptr_t size, std::string name);

private:
    int32_t tid_;
    std::vector<MemoryBlockInfo> blocks_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
