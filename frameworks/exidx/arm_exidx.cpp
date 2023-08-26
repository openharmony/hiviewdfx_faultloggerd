/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "arm_exidx.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxArmExidx"

#define ARM_EXIDX_CANT_UNWIND   0x00000001
#define ARM_EXIDX_COMPACT       0x80000000
#define ARM_EXTBL_OP_FINISH     0xb0
}

bool ArmExidx::ExtractEntryData(uintptr_t entryOffset)
{
    uint32_t data = 0;
    uintptr_t extabAddress = 0;
    if (!memory_->ReadPrel31(&entryOffset, &data)) {
        return false;
    }
    entryOffset += 4;
    if (memory_->ReadU32(&entryOffset, &data) != UNW_ERROR_NONE) {
        return false;
    }
    if (data == ARM_EXIDX_CANT_UNWIND) {
        lastErrorData_.code = UNW_ERROR_CANT_UNWIND;
        lastErrorData_.addr = entryOffset;
        return false;
    } else if ((data & ARM_EXIDX_COMPACT)) {
        if (((data >> 24) & 0x7f) != 0) {
            return false;
        }
        opCode.emplace((data & 0xff0000) >> 16);
        opCode.emplace((data & 0xff00) >> 8);
        opCode.emplace(data & 0xff);
    }
    uint8_t tableCount = 0;
    if ((data & ARM_EXIDX_COMPACT) == 0) {
        // prel31 decode point to .ARM.extab
        if (!memory_->ReadPrel31(&entryOffset, &extabAddress)) {
            return false;
        }
        if (memory_->ReadU32(&extabAddress, &data, false) != UNW_ERROR_NONE) {
            return false;
        }

        // Arm generic personality
        if ((data & ARM_EXIDX_COMPACT) == 0) {
            uintptr_t perRoutine;
            if (!memory_->ReadPrel31(&extabAddress, &perRoutine)) {
                return false;
            }
            extabAddress += 4;
            // Skip four bytes, because dont have unwind data to read
            if (memory_->ReadU32(&extabAddress, &data, false) != UNW_ERROR_NONE) {
                return false;
            }
            tableCount = (data >> 24) & 0xff;
            opCode.emplace((data >> 16) & 0xff);
            opCode.emplace((data >> 8) & 0xff);
            opCode.emplace(data & 0xff);

        } else {  // Arm compact personality
            if (data >> 28 != 8) {
                LOGE("incorrect Arm compact model, [31:28]bit must be 0x8(%x)", data >> 28);
                return false;
            }
            uint8_t personality = (data >> 24) & 0x3;
            if (personality > 2) {
                LOGE("incorrect Arm compact personality(%u)", personality);
                return false;
            }
            // inline compact model, when personality is 0
            if (personality == 0) {
                opCode.emplace((data >> 16) & 0xff);
            } else if (personality == 1 || personality == 2) {
                tableCount = (data >> 16) & 0xff;
            }
            opCode.emplace((data >> 8) & 0xff);
            opCode.emplace(data & 0xff);
        }
        if (tableCount > 5) {
            return false;
        }
    }

    for (size_t i = 0; i < tableCount; i++) {
        extabAddress += 4;
        if (memory_->ReadU32(&extabAddress, &data, false) != UNW_ERROR_NONE) {
            return false;
        }
        opCode.emplace((data >> 24) & 0xff);
        opCode.emplace((data >> 16) & 0xff);
        opCode.emplace((data >> 8) & 0xff);
        opCode.emplace(data & 0xff);
    }
    if (!opCode.empty() && opCode.back() != ARM_EXTBL_OP_FINISH) {
        opCode.emplace(ARM_EXTBL_OP_FINISH);
    }
    return true;
}

bool ArmExidx::Eval()
{
    DecodeTable decodeTable[] = {
        {0xc0, 0x00, &ArmExidx::Decode00xxxxxx},
        {0xc0, 0x40, &ArmExidx::Decode01xxxxxx},
        {0xf0, 0x80, &ArmExidx::Decode1000iiiiiiiiiiii},
        {0xff, 0x9d, &ArmExidx::Decode10011101},
        {0xff, 0x9f, &ArmExidx::Decode10011111},
        {0xf0, 0x90, &ArmExidx::Decode1001nnnn},
        {0xf8, 0xa0, &ArmExidx::Decode10100nnn},
        {0xf8, 0xa8, &ArmExidx::Decode10101nnn},
        {0xff, 0xb0, &ArmExidx::Decode10110000},
        {0xff, 0xb1, &ArmExidx::Decode101100010000iiii},
        {0xff, 0xb2, &ArmExidx::Decode10110010uleb128},
        {0xff, 0xb3, &ArmExidx::Decode10110011sssscccc},
        {0xfc, 0xb4, &ArmExidx::Decode101101nn},
        {0xf8, 0xb8, &ArmExidx::Decode10111nnn},
        {0xff, 0xc6, &ArmExidx::Decode11000110sssscccc},
        {0xff, 0xc7, &ArmExidx::Decode110001110000iiii},
        {0xff, 0xc8, &ArmExidx::Decode11001000sssscccc},
        {0xff, 0xc9, &ArmExidx::Decode11001001sssscccc},
        {0xc8, 0xc8, &ArmExidx::Decode11001yyy},
        {0xf8, 0xc0, &ArmExidx::Decode11000nnn},
        {0xf8, 0xd0, &ArmExidx::Decode11010nnn},
        {0xc0, 0xc0, &ArmExidx::Decode11xxxyyy}
    };
    cfa_ = (*regs_)[REG_SP];
    while (Decode(decodeTable, sizeof(decodeTable)));
    return lastErrorData_.code == UNW_ERROR_ARM_EXIDX_FINISH;
}
bool ArmExidx::PopRegsUnderMask(uint16_t mask)
{
    std::string msg = "Pop ";
    bool firstPrint = true;
    for (size_t i = 0; i < REG_ARM_LAST; i++) {
        if ((mask & (1 << i)) != 0) {
            if (!firstPrint) {
                msg += ", ";
            }
            msg += ("r" + std::to_string(i));
            firstPrint = false;
            if (!memory_->ReadU32(&cfa_, &(*regs_)[i], false)) {
                lastErrorData_.code = UNW_ERROR_INVALID_REGS;
                lastErrorData_.addr = cfa_;
                LOGE("Failed to pop r%zu", i);
                return false;
            }
            cfa_ += 4;
        }
    }
    LOGI("%s", msg.c_str());
    return true;
}

inline bool ArmExidx::Spare()
{
    LOGI("Spare");
    lastErrorData_.code = UNW_ERROR_ARM_EXIDX_SPARE;
    return false;
}

inline bool ArmExidx::StepOperateCode()
{
    if (opCode.empty()) {
        return false;
    }
    curCode = opCode.front();
    opCode.pop();
    return true;
}

inline bool ArmExidx::Decode(ArmExidx::DecodeTable decodeTable[], size_t size)
{
    if (!StepOperateCode()) {
        return false;
    }
    for (size_t i = 0 ; i < size ; ++i) {
        if ((curCode & decodeTable[i].mask) == decodeTable[i].result) {
            if (!decodeTable[i].decoder) {
                return false;
            }
        }
    }
    return true;
}
inline bool ArmExidx::Decode00xxxxxx()
{
    //vsp = vsp + (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive
    curCode = curCode + ((curCode & 0x3f) << 2) + 4;
    return true;
}
inline bool ArmExidx::Decode01xxxxxx()
{
    //vsp = vsp - (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive
    curCode = curCode - ((curCode & 0x3f) << 2) + 4;
    return true;
}
inline bool ArmExidx::Decode1000iiiiiiiiiiii()
{
    uint8_t prevCode = curCode;
    if (!StepOperateCode()) {
        return false;
    }
    uint16_t mask = ((prevCode & 0x0f) << 12) | (curCode << 4);
    if (mask == 0x0) { // 10000000 00000000: Refuse to unwind
        LOGE("Refuse to unwind!");
        lastErrorData_.code = UNW_ERROR_CANT_UNWIND;
        return false;
    }

    // 1000iiii iiiiiiii (i not all 0)
    if (!PopRegsUnderMask(mask)){
        return false;
    }

    if (mask & (1 << REG_PC)) {
        pcSet = true;
    }

    if (mask & (1 << REG_SP)) {
        cfa_ = (*regs_)[REG_SP];
    }
    return true;
}

inline bool ArmExidx::Decode10011101()
{
    //Reserved as prefix for ARM register to register moves
    LOGI("Reserved as prefix for ARM register to register moves");
    lastErrorData_.code = UNW_ERROR_RESERVED_VALUE;
    return false;
}

inline bool ArmExidx::Decode10011111()
{
    // Reserved as prefix for Intel Wireless MMX register to register moves
    LOGI("Reserved as prefix for Intel Wireless MMX register to register moves");
    lastErrorData_.code = UNW_ERROR_RESERVED_VALUE;
    return false;
}

inline bool ArmExidx::Decode1001nnnn()
{
    //Set vsp = r[nnnn]
    cfa_ = (*regs_)[curCode & 0xf];
    return true;
}

inline bool ArmExidx::Decode10100nnn()
{
    //10100nnn Pop r4-r[4+nnn]
    size_t startIndex = 4;
    size_t endIndex = 4 + (curCode & 0x7);
    std::string msg = "Pop r" + std::to_string(startIndex);
    if (startIndex < endIndex){
        msg += "-r" + std::to_string(endIndex);
    }
    for (size_t i = startIndex; i <= endIndex; i++) {
        if (!memory_->ReadU32(&cfa_, &(*regs_)[i], false)) {
            LOGE("Failed to pop r%zu", i);
            return false;
        }
        cfa_ += 4;
    }
    LOGI("%s", msg.c_str());
    return true;
}

inline bool ArmExidx::Decode10101nnn()
{
    // 10101nnn Pop r4-r[4+nnn], r14
    if(!Decode10100nnn()) {
        return false;
    }
    if (!memory_->ReadU32(&cfa_, &(*regs_)[REG_LR], false)) {
        return false;
    }
    LOGI("Pop r14");
    cfa_ += 4;
    return true;
}

inline bool ArmExidx::Decode10110000()
{
    LOGI("Finish");
    lastErrorData_.code = UNW_ERROR_ARM_EXIDX_FINISH;
    if (!pcSet) {
      (*regs_)[REG_PC] = (*regs_)[REG_LR];
    }
    (*regs_)[REG_SP] = cfa_;
    return false;
}

inline bool ArmExidx::Decode101100010000iiii()
{
    if (!StepOperateCode()) {
        return false;
    }
    // 10110001 00000000 || 10110001 xxxxyyyy spare
    if (curCode == 0x00 || (curCode & 0xf0) != 0) {
        return false;
    }
    uint8_t mask = curCode & 0x0f;
    // 10110001 0000iiii(i not all 0) Pop integer registers under mask{r3, r2,r1,r0}
    return PopRegsUnderMask(mask);
}
inline bool ArmExidx::Decode10110010uleb128()
{
    // 10110010 uleb128 vsp = vsp + 0x204 + (uleb128 << 2)
    std::vector<uint8_t> ulebBuf;
    uint8_t shift = 0;
    uint32_t uleb128 = 0;
    do {
        if (!StepOperateCode()) {
            return false;
        }
        uleb128 |= (curCode & 0x7f) << shift;
        shift += 7;
    } while ((curCode & 0x80) != 0);
    uint32_t offset = 0x204 + (uleb128 << 2);
    cfa_ = cfa_ + offset;
    LOGI("vsp = vsp + %d",offset);
    return true;
}

inline bool ArmExidx::Decode10110011sssscccc()
{
    // Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
    if (!StepOperateCode()) {
        return false;
    }
    uint8_t popRegCount = (curCode & 0x0f) + 1 ;
    cfa_ += popRegCount * 8 + 4;
    return true;

}

inline bool ArmExidx::Decode101101nn()
{
    return Spare();
}

inline bool ArmExidx::Decode10111nnn()
{
    // Pop VFP double-precision registers D[8]-D[8+nnn] by FSTMFDX
    uint8_t popRegCount = (curCode & 0x0f) + 1 ;
    cfa_ += popRegCount * 8 + 4;
    return true;
}

inline bool ArmExidx::Decode11000110sssscccc()
{
    // Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc] (see remark e)
    if (!StepOperateCode()) {
        return false;
    }
    return Decode11000nnn();
}

inline bool ArmExidx::Decode110001110000iiii()
{
    // Intel Wireless MMX pop wCGR registers under mask {wCGR3,2,1,0}
    if (!StepOperateCode()) {
        return false;
    }
    if ((curCode & 0xf0) != 0 || curCode == 0) {
        return Spare();
    }

    for (size_t i = 0; i < 4; i++)
    {
        if (curCode & (i << i)) {
            cfa_ += 4;
        }
    }
    return true;
}
inline bool ArmExidx::Decode11001000sssscccc()
{
    // Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
    if (!StepOperateCode()) {
        return false;
    }
    uint8_t popRegCount = (curCode & 0x0f) + 1;
    cfa_ += popRegCount * 8;    // by VPUSH not add 4
    return true;
}

inline bool ArmExidx::Decode11001001sssscccc()
{
    // Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
    return Decode11001000sssscccc();
}

inline bool ArmExidx::Decode11001yyy()
{
    return Spare();
}

inline bool ArmExidx::Decode11000nnn()
{
    // Intel Wireless MMX pop wR[10]-wR[10+nnn]
    uint8_t popRegCount = (curCode & 0x0f) + 1;
    cfa_ += popRegCount * 8;
    return true;
}

inline bool ArmExidx::Decode11010nnn()
{
    // Pop VFP double-precision registers D[8]-D[8+nnn] saved (as if) by VPUSH (seeremark d)
    uint8_t popRegCount = (curCode & 0x0f) + 1;
    cfa_ += popRegCount * 8;
    return true;
}

inline bool ArmExidx::Decode11xxxyyy()
{
    return Spare();
}
} // namespace HiviewDFX
} // namespace OHOS