/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef ARM_EXIDX_H
#define ARM_EXIDX_H

#include <deque>
#include <memory>
#include <vector>
#include "dfx_errors.h"
#include "dfx_regs.h"
#include "dfx_memory.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
struct ExidxContext {
public:
    int32_t vsp = 0;
    uint32_t transformedBits = 0;
    int32_t regs[REG_LAST] = {0};

    void Reset();
    void Transform(uint32_t reg);
    bool IsTransformed(uint32_t reg);
    void AddUpTransformed(uint32_t reg, int32_t imm);
    void AddUpVsp(int32_t imm);
};

class ArmExidx {
public:
    ArmExidx(std::shared_ptr<DfxMemory> memory) : memory_(memory)
    {
        lastErrorData_.code = UNW_ERROR_NONE;
        lastErrorData_.addr = 0;
    }
    virtual ~ArmExidx() = default;

    bool SearchEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti, uintptr_t pc);
    bool Step(uintptr_t entryOffset, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);

    const uint16_t& GetLastErrorCode() const { return lastErrorData_.code; }
    const uint64_t& GetLastErrorAddr() const { return lastErrorData_.addr; }

private:
    struct DecodeTable {
        uint8_t mask;
        uint8_t result;
        bool (ArmExidx::*decoder)();
    };

    bool Eval(uintptr_t entryOffset);
    void FlushInstr();

    void LogRawData();
    bool ExtractEntryData(uintptr_t entryOffset);
    bool GetOpCode();
    bool Decode(DecodeTable decodeTable[], size_t size);
    bool Decode00xxxxxx();
    bool Decode01xxxxxx();
    bool Decode1000iiiiiiiiiiii();
    bool Decode1001nnnn();
    bool Decode1010nnnn();
    bool Decode10110000();
    bool Decode101100010000iiii();
    bool Decode10110010uleb128();
    bool Decode10110011sssscccc();
    bool Decode101101nn();
    bool Decode10111nnn();
    bool Decode11000110sssscccc();
    bool Decode110001110000iiii();
    bool Decode1100100nsssscccc();
    bool Decode11001yyy();
    bool Decode11000nnn();
    bool Decode11010nnn();
    bool Decode11xxxyyy();
    bool DecodeSpare();

protected:
    UnwindErrorData lastErrorData_;
    std::shared_ptr<RegLocState> rsState_;
    std::shared_ptr<DfxMemory> memory_;
    ExidxContext context_;
    std::deque<uint8_t> ops_;
    uint8_t curOp_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
