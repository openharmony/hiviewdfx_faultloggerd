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
#include "dfx_instructions.h"
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

void ExidxContext::reset()
{
    vsp = 0;
    transformedBits = 0;
    regs.resize(QUT_MINI_REGS_SIZE);
}

void ExidxContext::Transform(uint32_t reg)
{
    LOGU("Transform reg: %d", reg);
    transformedBits = transformedBits | (1 << reg);
    regs[reg] = 0;
}

bool ExidxContext::IsTransformed(uint32_t reg)
{
    if (transformedBits & (1 << reg)) {
        return true;
    }
    return false;
}

void ExidxContext::AddUpTransformed(uint32_t reg, int32_t imm)
{
    if (IsTransformed(reg)) {
        LOGU("AddUpTransformed reg: %d, imm: %d", reg, imm);
        regs[reg] += imm;
    }
}

void ExidxContext::AddUpVsp(int32_t imm)
{
    LOGU("AddUpVsp imm: %d", imm);

    vsp += imm;

    AddUpTransformed(QUT_REG_R7, imm);
    AddUpTransformed(QUT_REG_R11, imm);
    AddUpTransformed(QUT_REG_SP, imm);
    AddUpTransformed(QUT_REG_LR, imm);
    AddUpTransformed(QUT_REG_PC, imm);
}

inline void ArmExidx::FlushInstr()
{
    if (context_.vsp != 0) {
        LOG_CHECK((context_.vsp & 0x3) == 0);
        rsState_->cfaReg = REG_SP;
        rsState_->cfaRegOffset = context_.vsp;
        LOGU("rsState cfaRegOffset: %d", rsState_->cfaRegOffset);
    }

    // Sort by vsp offset ascend, convenient for search prologue later.
    std::vector<std::pair<size_t, int32_t>> sortedRegs;
    for (size_t i = 0; i < QUT_MINI_REGS_SIZE; i++) {
        if (!(context_.IsTransformed(i))) {
            continue;
        }
        int32_t vspOffset = context_.regs[i];
        size_t insertPos = 0;
        for (size_t j = 0; j < sortedRegs.size(); j++) {
            if (vspOffset >= 0 &&
                (sortedRegs.at(j).second > vspOffset || sortedRegs.at(j).second < 0)) {
                break;
            }
            insertPos = j + 1;
        }
        sortedRegs.insert(sortedRegs.begin() + insertPos, std::make_pair(i, vspOffset));
        LOGU("[%d], i: %d, vspOffset: %d", insertPos, i, vspOffset);
    }
    for (size_t j = 0; j < sortedRegs.size(); j++) {
        uint32_t reg = (sortedRegs.at(j).first);
        if (context_.IsTransformed(reg)) {
            LOG_CHECK((context_.regs[reg] & 0x3) == 0);

            rsState_->locs[reg].type = REG_LOC_MEM_OFFSET;
            rsState_->locs[reg].val = -context_.regs[reg];
            LOGU("rsState locs[%d].type: %d, val: %d", reg, rsState_->locs[reg].type, rsState_->locs[reg].val);
        }
    }

    context_.reset();
}

inline bool ArmExidx::ApplyInstr()
{
    FlushInstr();

    DfxInstructions instructions(memory_);
    bool ret = instructions.Apply(regs_, rsState_);

    if (!isPcSet_) {
        regs_->SetReg(REG_PC, regs_->GetReg(REG_LR));
    }
    return ret;
}

inline void ArmExidx::LogRawData()
{
    std::string logStr("Raw Data:");
    for (const uint8_t data : ops_) {
        logStr += StringPrintf(" 0x%02x", data);
    }
    LOGU("%s", logStr.c_str());
}

bool ArmExidx::ExtractEntryData(uintptr_t entryOffset)
{
    LOGU("Exidx entryOffset: %llx", (uint64_t)entryOffset);
    ops_.clear();
    uint32_t data = 0;
    lastErrorData_.code = UNW_ERROR_NONE;
    lastErrorData_.addr = entryOffset;
    if (entryOffset & 1) {
        lastErrorData_.code = UNW_ERROR_INVALID_ALIGNMENT;
        return false;
    }

    entryOffset += 4;
    if (!memory_->ReadU32(entryOffset, &data, false)) {
        lastErrorData_.addr = entryOffset;
        return false;
    }

    if (data == ARM_EXIDX_CANT_UNWIND) {
        LOGU("This is a CANT UNWIND entry.");
        lastErrorData_.code = UNW_ERROR_CANT_UNWIND;
        lastErrorData_.addr = entryOffset;
        return false;
    } else if ((data & ARM_EXIDX_COMPACT)) {
        if (((data >> 24) & 0x7f) != 0) {
            LOGU("This is a non-zero index, this code doesn't support other formats.");
            lastErrorData_.code = UNW_ERROR_INVALID_PERSONALITY;
            return false;
        }
        ops_.push_back((data >> 16) & 0xff);
        ops_.push_back((data >> 8) & 0xff);
        uint8_t lastOp = data & 0xff;
        ops_.push_back(lastOp);
        if (lastOp != ARM_EXTBL_OP_FINISH) {
            ops_.push_back(ARM_EXTBL_OP_FINISH);
        }
        LogRawData();
        return true;
    }

    uintptr_t extabAddr = 0;
    // prel31 decode point to .ARM.extab
    if (!memory_->ReadPrel31(entryOffset, &extabAddr)) {
        lastErrorData_.code = UNW_ERROR_INVALID_MEMORY;
        return false;
    }
    if (!memory_->ReadU32(extabAddr, &data, false)) {
        lastErrorData_.code = UNW_ERROR_INVALID_MEMORY;
        return false;
    }

    uint8_t tableCount = 0;
    if ((data & ARM_EXIDX_COMPACT) == 0) {
        LOGU("Arm generic personality.");
        uintptr_t perRoutine;
        if (!memory_->ReadPrel31(extabAddr, &perRoutine)) {
            return false;
        }
        extabAddr += 4;
        // Skip four bytes, because dont have unwind data to read
        if (!memory_->ReadU32(extabAddr, &data, false)) {
            lastErrorData_.code = UNW_ERROR_INVALID_MEMORY;
            lastErrorData_.addr = extabAddr;
            return false;
        }
        tableCount = (data >> 24) & 0xff;
        ops_.push_back((data >> 16) & 0xff);
        ops_.push_back((data >> 8) & 0xff);
        ops_.push_back(data & 0xff);
        extabAddr += 4;
    } else {
        LOGU("Arm compact personality.");
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
            ops_.push_back((data >> 16) & 0xff);
        } else if (personality == 1 || personality == 2) {
            tableCount = (data >> 16) & 0xff;
            extabAddr += 4;
        }
        ops_.push_back((data >> 8) & 0xff);
        ops_.push_back(data & 0xff);
    }
    if (tableCount > 5) {
        lastErrorData_.code = UNW_ERROR_NOT_SUPPORT;
        return false;
    }

    for (size_t i = 0; i < tableCount; i++) {
        if (!memory_->ReadU32(extabAddr, &data, false)) {
            return false;
        }
        extabAddr += 4;
        ops_.push_back((data >> 24) & 0xff);
        ops_.push_back((data >> 16) & 0xff);
        ops_.push_back((data >> 8) & 0xff);
        ops_.push_back(data & 0xff);
    }

    if (!ops_.empty() && ops_.back() != ARM_EXTBL_OP_FINISH) {
        ops_.push_back(ARM_EXTBL_OP_FINISH);
    }
    LogRawData();
    return true;
}

inline bool ArmExidx::StepOpCode()
{
    if (ops_.empty()) {
        return false;
    }
    curOp_ = ops_.front();
    ops_.pop_front();
    return true;
}

bool ArmExidx::Eval(uintptr_t entryOffset, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    regs_ = regs;
    rsState_ = rs;
    if (!ExtractEntryData(entryOffset)) {
        return false;
    }

    DecodeTable decodeTable[] = {
        {0xc0, 0x00, &ArmExidx::Decode00xxxxxx},
        {0xc0, 0x40, &ArmExidx::Decode01xxxxxx},
        {0xf0, 0x80, &ArmExidx::Decode1000iiiiiiiiiiii},
        {0xf0, 0x90, &ArmExidx::Decode1001nnnn},
        {0xf0, 0x0a, &ArmExidx::Decode1010nnnn},
        {0xff, 0xb0, &ArmExidx::Decode10110000},
        {0xff, 0xb1, &ArmExidx::Decode101100010000iiii},
        {0xff, 0xb2, &ArmExidx::Decode10110010uleb128},
        {0xff, 0xb3, &ArmExidx::Decode10110011sssscccc},
        {0xfc, 0xb4, &ArmExidx::Decode101101nn},
        {0xf8, 0xb8, &ArmExidx::Decode10111nnn},
        {0xff, 0xc6, &ArmExidx::Decode11000110sssscccc},
        {0xff, 0xc7, &ArmExidx::Decode110001110000iiii},
        {0xfe, 0xc8, &ArmExidx::Decode1100100nsssscccc},
        {0xc8, 0xc8, &ArmExidx::Decode11001yyy},
        {0xf8, 0xc0, &ArmExidx::Decode11000nnn},
        {0xf8, 0xd0, &ArmExidx::Decode11010nnn},
        {0xc0, 0xc0, &ArmExidx::Decode11xxxyyy}
    };
    context_.reset();
    while (Decode(decodeTable, sizeof(decodeTable)));

    return ApplyInstr();
}

inline bool ArmExidx::DecodeSpare()
{
    LOGU("Exdix Decode Spare");
    lastErrorData_.code = UNW_ERROR_ARM_EXIDX_SPARE;
    return false;
}

inline bool ArmExidx::Decode(DecodeTable decodeTable[], size_t size)
{
    if (!StepOpCode()) {
        return false;
    }

    for (size_t i = 0 ; i < size ; ++i) {
        if ((curOp_ & decodeTable[i].mask) == decodeTable[i].result) {
            if (!decodeTable[i].decoder) {
                return false;
            }
        }
    }
    return true;
}

inline bool ArmExidx::Decode00xxxxxx()
{
    // 00xxxxxx: vsp = vsp + (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive
    context_.AddUpVsp(((curOp_ & 0x3f) << 2) + 4);
    return true;
}

inline bool ArmExidx::Decode01xxxxxx()
{
    // 01xxxxxx: vsp = vsp - (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive
    context_.AddUpVsp(-(((curOp_ & 0x3f) << 2) + 4));
    return true;
}

inline bool ArmExidx::Decode1000iiiiiiiiiiii()
{
    uint16_t registers = ((curOp_ & 0x0f) << 8);
    if (!StepOpCode()) {
        return false;
    }
    registers |= curOp_;
    if (registers == 0x0) {
        LOGE("10000000 00000000: Refuse to unwind!");
        lastErrorData_.code = UNW_ERROR_CANT_UNWIND;
        return false;
    }

    registers <<= 4;
    LOGU("1000iiii iiiiiiii: registers: %x)", registers);
    for (size_t reg = REG_ARM_R4; reg < REG_ARM_LAST; reg++) {
        if ((registers & (1 << reg))) {
            if (REG_ARM_R7 == reg) {
                context_.Transform(QUT_REG_R7);
            } else if (REG_ARM_R11 == reg) {
                context_.Transform(QUT_REG_R11);
            } else if (REG_SP == reg) {
                context_.Transform(QUT_REG_SP);
            } else if (REG_LR == reg) {
                context_.Transform(QUT_REG_LR);
            } else if (REG_PC == reg) {
                isPcSet_ = true;
                context_.Transform(QUT_REG_PC);
            }
            context_.AddUpVsp(4);
        }
    }
    return true;
}

inline bool ArmExidx::Decode1001nnnn()
{
    uint8_t bits = curOp_ & 0xf;
    if (bits == 13 || bits == 15) {
        LOGU("10011101 or 10011111: Reserved");
        lastErrorData_.code = UNW_ERROR_RESERVED_VALUE;
        return false;
    }
    // 1001nnnn: Set vsp = r[nnnn]
    if ((bits == REG_ARM_R7) || (bits == REG_ARM_R11)) {
        LOGU("1001nnnn: Set vsp = %d", bits);
        if (context_.transformedBits == 0) {
            // No register transformed, ignore vsp offset.
            context_.reset();
        }
        rsState_->cfaReg = bits;
        rsState_->cfaRegOffset = 0;
    } else {
        LOGE("1001nnnn: Failed to set vsp = %d", bits);
        return false;
    }
    return true;
}

inline bool ArmExidx::Decode1010nnnn()
{
    // 10100nnn: Pop r4-r[4+nnn]
    // 10101nnn: Pop r4-r[4+nnn], r14
    size_t startReg = REG_ARM_R4;
    size_t endReg = REG_ARM_R4 + (curOp_ & 0x7);
    std::string msg = "Pop r" + std::to_string(startReg);
    if (endReg > startReg) {
        msg += "-r" + std::to_string(endReg);
    }
    if (curOp_ & 0x8) {
        msg += ", r14";
    }
    LOGU("%s", msg.c_str());

    for (size_t reg = startReg; reg <= endReg; reg++) {
        if (reg == REG_ARM_R7) {
            context_.Transform(QUT_REG_R7);
        } else if (reg == REG_ARM_R11) {
            context_.Transform(QUT_REG_R11);
        }
        context_.AddUpVsp(4);
    }

    if (curOp_ & 0x8) {
        context_.Transform(QUT_REG_LR);
        context_.AddUpVsp(4);
    }
    return true;
}

inline bool ArmExidx::Decode10110000()
{
    LOGU("10110000: Finish");
    lastErrorData_.code = UNW_ERROR_ARM_EXIDX_FINISH;
    return true;
}

inline bool ArmExidx::Decode101100010000iiii()
{
    if (!StepOpCode()) {
        return false;
    }
    // 10110001 00000000: spare
    // 10110001 xxxxyyyy: spare (xxxx != 0000)
    if (curOp_ == 0x00 || (curOp_ & 0xf0) != 0) {
        return DecodeSpare();
    }

    // 10110001 0000iiii(i not all 0) Pop integer registers under mask{r3, r2, r1, r0}
    uint8_t registers = curOp_ & 0x0f;
    for (size_t reg = 0; reg < 4; reg++) {
        if ((registers & (1 << reg))) {
            context_.AddUpVsp(4);
        }
    }
    return true;
}

inline bool ArmExidx::Decode10110010uleb128()
{
    // 10110010 uleb128 vsp = vsp + 0x204 + (uleb128 << 2)
    uint8_t shift = 0;
    uint32_t uleb128 = 0;
    do {
        if (!StepOpCode()) {
            return false;
        }
        uleb128 |= (curOp_ & 0x7f) << shift;
        shift += 7;
    } while ((curOp_ & 0x80) != 0);
    uint32_t offset = 0x204 + (uleb128 << 2);
    LOGU("vsp = vsp + %d", offset);
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode10110011sssscccc()
{
    // Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
    if (!StepOpCode()) {
        return false;
    }
    uint8_t popRegCount = (curOp_ & 0x0f) + 1;
    uint32_t offset = popRegCount * 8 + 4;
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode101101nn()
{
    return DecodeSpare();
}

inline bool ArmExidx::Decode10111nnn()
{
    // 10111nnn: Pop VFP double-precision registers D[8]-D[8+nnn] by FSTMFDX
    uint8_t popRegCount = (curOp_ & 0x0f) + 1;
    uint32_t offset = popRegCount * 8 + 4;
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode11000110sssscccc()
{
    // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc] (see remark e)
    if (!StepOpCode()) {
        return false;
    }
    return Decode11000nnn();
}

inline bool ArmExidx::Decode110001110000iiii()
{
    // Intel Wireless MMX pop wCGR registers under mask {wCGR3,2,1,0}
    if (!StepOpCode()) {
        return false;
    }
    // 11000111 00000000: Spare
    // 11000111 xxxxyyyy: Spare (xxxx != 0000)
    if ((curOp_ & 0xf0) != 0 || curOp_ == 0) {
        return DecodeSpare();
    }

    // 11000111 0000iiii: Intel Wireless MMX pop wCGR registers {wCGR0,1,2,3}
    for (size_t i = 0; i < 4; i++) {
        if (curOp_ & (1 << i)) {
            context_.AddUpVsp(4);
        }
    }
    return true;
}

inline bool ArmExidx::Decode1100100nsssscccc()
{
    // 11001000 sssscccc: Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
    // 11001001 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
    if (!StepOpCode()) {
        return false;
    }
    uint8_t popRegCount = (curOp_ & 0x0f) + 1;
    uint32_t offset = popRegCount * 8;
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode11001yyy()
{
    // 11001yyy: Spare (yyy != 000, 001)
    return DecodeSpare();
}

inline bool ArmExidx::Decode11000nnn()
{
    // Intel Wireless MMX pop wR[10]-wR[10+nnn]
    uint8_t popRegCount = (curOp_ & 0x0f) + 1;
    uint32_t offset = popRegCount * 8;
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode11010nnn()
{
    // Pop VFP double-precision registers D[8]-D[8+nnn] saved (as if) by VPUSH (seeremark d)
    uint8_t popRegCount = (curOp_ & 0x0f) + 1;
    uint32_t offset = popRegCount * 8;
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode11xxxyyy()
{
    // 11xxxyyy: Spare (xxx != 000, 001, 010)
    return DecodeSpare();
}
} // namespace HiviewDFX
} // namespace OHOS