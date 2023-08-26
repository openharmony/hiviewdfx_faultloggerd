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

#include <queue>
#include <memory>
#include <vector>
#include "dfx_errors.h"
#include "dfx_regs.h"
#include "dfx_memory.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {

class ArmExidx {
public:
    ArmExidx(DfxMemory* memory, DfxRegsArm* regs) : memory_(memory), regs_(regs)  {}
    virtual ~ArmExidx() = default;
    struct DecodeTable {
        uint8_t mask;
        uint8_t result;
        bool (ArmExidx::*decoder)();
    };

    bool ExtractEntryData(uintptr_t entryOffset);
    bool Decode(DecodeTable decodeTable[], size_t size);
    bool Eval();
    std::queue<uint8_t> GetData() { return opCode; }
    void SetData(std::queue<uint8_t> &data) { opCode = data ; }
    UnwindErrorData lastErrorData_;
private:
    bool StepOperateCode();
    bool Decode00xxxxxx();
    bool Decode01xxxxxx();
    bool Decode1000iiiiiiiiiiii();
    bool Decode10011101();
    bool Decode10011111();
    bool Decode1001nnnn();
    bool Decode10100nnn();
    bool Decode10101nnn();
    bool Decode10110000();
    bool Decode101100010000iiii();
    bool Decode10110010uleb128();
    bool Decode10110011sssscccc();
    bool Decode101101nn();
    bool Decode10111nnn();
    bool Decode11000110sssscccc();
    bool Decode110001110000iiii();
    bool Decode11001000sssscccc();
    bool Decode11001001sssscccc();
    bool Decode11001yyy();
    bool Decode11000nnn();
    bool Decode11010nnn();
    bool Decode11xxxyyy();
    bool Spare();
    bool PopRegsUnderMask(uint16_t mask);

    uint8_t curCode;

    bool pcSet = false;

protected:
    DfxMemory* memory_;
    std::queue<uint8_t> opCode;
    uintptr_t cfa_ = 0;
    DfxRegsArm* regs_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
