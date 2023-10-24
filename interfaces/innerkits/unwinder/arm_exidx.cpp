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
#include "dfx_regs.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "string_printf.h"

#if defined(__arm__)

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

static const uintptr_t EXIDX_ENTRY_SIZE = 8;
static const uintptr_t FOUR_BYTE_OFFSET = 4;
static const int TWO_BIT_OFFSET = 2;
static const int FOUR_BIT_OFFSET = 4;
static const int SEVEN_BIT_OFFSET = 7;
static const int EIGHT_BIT_OFFSET = 8;
static const int SIXTEEN_BIT_OFFSET = 16;
static const int TWENTY_FOUR_BIT_OFFSET = 24;
static const int TWENTY_EIGHT_BIT_OFFSET = 28;
}

void ExidxContext::Reset()
{
    vsp = 0;
    transformedBits = 0;
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

    auto qutRegs = DfxRegs::GetQutRegs();
    for (size_t i = 0; i < qutRegs.size(); i++) {
        AddUpTransformed(qutRegs[i], imm);
    }
}

inline void ArmExidx::FlushInstr()
{
    if (rsState_->cfaReg == 0) {
        rsState_->cfaReg = REG_SP;
    }
    rsState_->cfaRegOffset = 0;
    if (context_.vsp != 0) {
        LOG_CHECK((context_.vsp & 0x3) == 0);
        rsState_->cfaRegOffset = context_.vsp;
    }
    LOGU("rsState cfaReg: %d, cfaRegOffset: %d", rsState_->cfaReg, rsState_->cfaRegOffset);

    auto qutRegs = DfxRegs::GetQutRegs();
    for (size_t i = 0; i < qutRegs.size(); i++) {
        size_t reg = qutRegs[i];
        if (context_.IsTransformed(reg)) {
            LOG_CHECK((context_.regs[reg] & 0x3) == 0);
            rsState_->locs[reg].type = REG_LOC_MEM_OFFSET;
            rsState_->locs[reg].val = -context_.regs[reg];
            LOGU("rsState locs[%d].type: %d, val: %d", reg, rsState_->locs[reg].type, rsState_->locs[reg].val);
        }
    }

    context_.Reset();
}

inline void ArmExidx::LogRawData()
{
    std::string logStr("Raw Data:");
    for (const uint8_t data : ops_) {
        logStr += StringPrintf(" 0x%02x", data);
    }
    LOGU("%s", logStr.c_str());
}

bool ArmExidx::SearchEntry(uintptr_t pc, struct UnwindTableInfo uti, struct UnwindEntryInfo& uei)
{
    uintptr_t first = uti.tableData;
    uintptr_t last = uti.tableData + uti.tableLen - ARM_EXIDX_TABLE_SIZE;
    LOGU("SearchEntry pc:%p, tableData: %llx", (void*)pc, (uint64_t)first);
    uintptr_t entry, val;

    if (!memory_->ReadPrel31(first, &val) || pc < val) {
        lastErrorData_.addr = first;
        lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
        return false;
    }
    if (!memory_->ReadPrel31(last, &val)) {
        lastErrorData_.addr = last;
        lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
        return false;
    }

    if (pc >= val) {
        entry = last;
        if (!memory_->ReadPrel31(entry, &(uei.startPc))) {
            lastErrorData_.addr = entry;
            lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
            return false;
        }
        uei.endPc = uti.endPc - 1;
    } else {
        while (first < last - EXIDX_ENTRY_SIZE) {
            entry = first + (((last - first) / ARM_EXIDX_TABLE_SIZE + 1) >> 1) * ARM_EXIDX_TABLE_SIZE;
            if (!memory_->ReadPrel31(entry, &val)) {
                lastErrorData_.addr = entry;
                lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
                return false;
            }
            if (pc < val) {
                last = entry;
            } else {
                first = entry;
            }
        }
        entry = first;

        uintptr_t cur = entry;
        if (!memory_->ReadPrel31(cur, &(uei.startPc))) {
            lastErrorData_.addr = cur;
            lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
            return false;
        }

        cur = entry + EXIDX_ENTRY_SIZE;
        if (!memory_->ReadPrel31(cur, &(uei.endPc))) {
            lastErrorData_.addr = cur;
            lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
            return false;
        }

        uei.endPc--;
    }

    uei.gp = uti.gp;
    uei.unwindInfoSize = ARM_EXIDX_TABLE_SIZE;
    uei.unwindInfo = (void *) entry;
    uei.format = UNW_INFO_FORMAT_ARM_EXIDX;
    return true;
}

bool ArmExidx::ExtractEntryData(uintptr_t entryOffset)
{
    LOGU("Exidx entryOffset: %llx", (uint64_t)entryOffset);
    ops_.clear();
    uint32_t data = 0;
    lastErrorData_.addr = entryOffset;
    if (entryOffset & 1) {
        LOGE("entryOffset: %llx error.", (uint64_t)entryOffset);
        lastErrorData_.code = UNW_ERROR_INVALID_ALIGNMENT;
        return false;
    }

    entryOffset += FOUR_BYTE_OFFSET;
    if (!memory_->ReadU32(entryOffset, &data, false)) {
        LOGE("entryOffset: %llx error.", (uint64_t)entryOffset);
        lastErrorData_.addr = entryOffset;
        lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
        return false;
    }

    if (data == ARM_EXIDX_CANT_UNWIND) {
        LOGU("This is a CANT UNWIND entry, data: %x.", data);
        lastErrorData_.code = UNW_ERROR_CANT_UNWIND;
        lastErrorData_.addr = entryOffset;
        return false;
    } else if ((data & ARM_EXIDX_COMPACT) != 0) {
        if (((data >> TWENTY_FOUR_BIT_OFFSET) & 0x7f) != 0) {
            LOGE("This is a non-zero index, this code doesn't support other formats.");
            lastErrorData_.code = UNW_ERROR_INVALID_PERSONALITY;
            return false;
        }
        LOGU("This is a compact table entry, data: %x.", data);
        ops_.push_back((data >> SIXTEEN_BIT_OFFSET) & 0xff);
        ops_.push_back((data >> EIGHT_BIT_OFFSET) & 0xff);
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
#ifndef UNITTEST
    if (!memory_->ReadPrel31(entryOffset, &extabAddr)) {
        lastErrorData_.code = UNW_ERROR_INVALID_MEMORY;
        return false;
    }
#else
    extabAddr = entryOffset + FOUR_BYTE_OFFSET;
#endif
    if (!memory_->ReadU32(extabAddr, &data, false)) {
        lastErrorData_.code = UNW_ERROR_INVALID_MEMORY;
        return false;
    }

    uint8_t tableCount = 0;
    if ((data & ARM_EXIDX_COMPACT) == 0) {
        LOGU("Arm generic personality, data: %x.", data);
#ifndef UNITTEST
        uintptr_t perRoutine;
        if (!memory_->ReadPrel31(extabAddr, &perRoutine)) {
            LOGE("Arm Personality routine error");
            return false;
        }
#endif // !UNITTEST

        extabAddr += FOUR_BYTE_OFFSET;
        // Skip four bytes, because dont have unwind data to read
        if (!memory_->ReadU32(extabAddr, &data, false)) {
            lastErrorData_.code = UNW_ERROR_INVALID_MEMORY;
            lastErrorData_.addr = extabAddr;
            return false;
        }
        tableCount = (data >> TWENTY_FOUR_BIT_OFFSET) & 0xff;
        ops_.push_back((data >> SIXTEEN_BIT_OFFSET) & 0xff);
        ops_.push_back((data >> EIGHT_BIT_OFFSET) & 0xff);
        ops_.push_back(data & 0xff);
        extabAddr += FOUR_BYTE_OFFSET;
    } else {
        LOGU("Arm compact personality, data: %x.", data);
        if ((data >> TWENTY_EIGHT_BIT_OFFSET) != 0x8) {
            LOGE("incorrect Arm compact model, [31:28]bit must be 0x8(%x)", data >> TWENTY_EIGHT_BIT_OFFSET);
            return false;
        }
        uint8_t personality = (data >> TWENTY_FOUR_BIT_OFFSET) & 0x3;
        if (personality > 2) { // 2 : personality must be 0 1 2
            LOGE("incorrect Arm compact personality(%u)", personality);
            return false;
        }
        // inline compact model, when personality is 0
        if (personality == 0) {
            ops_.push_back((data >> SIXTEEN_BIT_OFFSET) & 0xff);
        } else if (personality == 1 || personality == 2) { // 2 : personality equal to 2
            tableCount = (data >> SIXTEEN_BIT_OFFSET) & 0xff;
            extabAddr += FOUR_BYTE_OFFSET;
        }
        ops_.push_back((data >> EIGHT_BIT_OFFSET) & 0xff);
        ops_.push_back(data & 0xff);
    }
    if (tableCount > 5) { // 5 : 5 operators
        lastErrorData_.code = UNW_ERROR_NOT_SUPPORT;
        return false;
    }

    for (size_t i = 0; i < tableCount; i++) {
        if (!memory_->ReadU32(extabAddr, &data, false)) {
            return false;
        }
        extabAddr += FOUR_BYTE_OFFSET;
        ops_.push_back((data >> TWENTY_FOUR_BIT_OFFSET) & 0xff);
        ops_.push_back((data >> SIXTEEN_BIT_OFFSET) & 0xff);
        ops_.push_back((data >> EIGHT_BIT_OFFSET) & 0xff);
        ops_.push_back(data & 0xff);
    }

    if (!ops_.empty() && ops_.back() != ARM_EXTBL_OP_FINISH) {
        ops_.push_back(ARM_EXTBL_OP_FINISH);
    }
    LogRawData();
    return true;
}

inline bool ArmExidx::GetOpCode()
{
    if (ops_.empty()) {
        return false;
    }
    curOp_ = ops_.front();
    ops_.pop_front();
    LOGU("curOp: %llx", (uint64_t)curOp_);
    return true;
}

bool ArmExidx::Eval(uintptr_t entryOffset)
{
    if (!ExtractEntryData(entryOffset)) {
        return false;
    }

    DecodeTable decodeTable[] = {
        {0xc0, 0x00, &ArmExidx::Decode00xxxxxx},
        {0xc0, 0x40, &ArmExidx::Decode01xxxxxx},
        {0xf0, 0x80, &ArmExidx::Decode1000iiiiiiiiiiii},
        {0xf0, 0x90, &ArmExidx::Decode1001nnnn},
        {0xf0, 0xa0, &ArmExidx::Decode1010nnnn},
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
    context_.Reset();
    while (Decode(decodeTable, sizeof(decodeTable) / sizeof(decodeTable[0])));
    return true;
}

bool ArmExidx::Step(uintptr_t entryOffset, std::shared_ptr<RegLocState> rs)
{
    if (rs == nullptr) {
        return false;
    }
    rsState_ = rs;

    if (!Eval(entryOffset)) {
        return false;
    }

    FlushInstr();
    return true;
}

inline bool ArmExidx::DecodeSpare()
{
    LOGU("Exidx Decode Spare");
    lastErrorData_.code = UNW_ERROR_ARM_EXIDX_SPARE;
    return false;
}

inline bool ArmExidx::Decode(DecodeTable decodeTable[], size_t size)
{
    if (!GetOpCode()) {
        return false;
    }

    bool ret = false;
    for (size_t i = 0; i < size; ++i) {
        if ((curOp_ & decodeTable[i].mask) == decodeTable[i].result) {
            if (decodeTable[i].decoder != nullptr) {
                LOGU("decodeTable[%d].mask: %02x", i, decodeTable[i].mask);
                ret = (this->*(decodeTable[i].decoder))();
                break;
            }
        }
    }
    return ret;
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
    if (!GetOpCode()) {
        return false;
    }
    registers |= curOp_;
    if (registers == 0x0) {
        LOGE("10000000 00000000: Refuse to unwind!");
        lastErrorData_.code = UNW_ERROR_CANT_UNWIND;
        return false;
    }

    registers <<= FOUR_BIT_OFFSET;
    LOGU("1000iiii iiiiiiii: registers: %02x)", registers);
    for (size_t reg = REG_ARM_R4; reg < REG_ARM_LAST; reg++) {
        if ((registers & (1 << reg))) {
            if (REG_PC == reg) {
                rsState_->isPcSet = true;
            }
            context_.Transform(reg);
            context_.AddUpVsp(FOUR_BYTE_OFFSET);
        }
    }
    return true;
}

inline bool ArmExidx::Decode1001nnnn()
{
    uint8_t bits = curOp_ & 0xf;
    if (bits == REG_ARM_R13 || bits == REG_ARM_R15) {
        LOGU("10011101 or 10011111: Reserved");
        lastErrorData_.code = UNW_ERROR_RESERVED_VALUE;
        return false;
    }
    // 1001nnnn: Set vsp = r[nnnn]
    if ((bits == REG_ARM_R7) || (bits == REG_ARM_R11)) {
        LOGU("1001nnnn: Set vsp = R%d", bits);
        if (context_.transformedBits == 0) {
            // No register transformed, ignore vsp offset.
            context_.Reset();
        }
    }
    rsState_->cfaReg = bits;
    rsState_->cfaRegOffset = 0;
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
        context_.Transform(reg);
        context_.AddUpVsp(FOUR_BYTE_OFFSET);
    }

    if (curOp_ & 0x8) {
        context_.Transform(REG_LR);
        context_.AddUpVsp(FOUR_BYTE_OFFSET);
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
    if (!GetOpCode()) {
        return false;
    }
    // 10110001 00000000: spare
    // 10110001 xxxxyyyy: spare (xxxx != 0000)
    if (curOp_ == 0x00 || (curOp_ & 0xf0) != 0) {
        return DecodeSpare();
    }

    // 10110001 0000iiii(i not all 0) Pop integer registers under mask{r3, r2, r1, r0}
    uint8_t registers = curOp_ & 0x0f;
    LOGU("10110001 0000iiii, registers: %02x", registers);
    for (size_t reg = 0; reg < 4; reg++) { // 4 : four registers {r3, r2, r1, r0}
        if ((registers & (1 << reg))) {
            context_.AddUpVsp(FOUR_BYTE_OFFSET);
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
        if (!GetOpCode()) {
            return false;
        }
        uleb128 |= (curOp_ & 0x7f) << shift;
        shift += SEVEN_BIT_OFFSET;
    } while ((curOp_ & 0x80) != 0);
    uint32_t offset = 0x204 + (uleb128 << TWO_BIT_OFFSET);
    LOGU("vsp = vsp + %d", offset);
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode10110011sssscccc()
{
    // Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
    if (!GetOpCode()) {
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
    uint8_t popRegCount = (curOp_ & 0x07) + 1;
    uint32_t offset = popRegCount * 8 + 4;
    context_.AddUpVsp(offset);
    return true;
}

inline bool ArmExidx::Decode11000110sssscccc()
{
    // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc] (see remark e)
    if (!GetOpCode()) {
        return false;
    }
    return Decode11000nnn();
}

inline bool ArmExidx::Decode110001110000iiii()
{
    // Intel Wireless MMX pop wCGR registers under mask {wCGR3,2,1,0}
    if (!GetOpCode()) {
        return false;
    }
    // 11000111 00000000: Spare
    // 11000111 xxxxyyyy: Spare (xxxx != 0000)
    if ((curOp_ & 0xf0) != 0 || curOp_ == 0) {
        return DecodeSpare();
    }

    // 11000111 0000iiii: Intel Wireless MMX pop wCGR registers {wCGR0,1,2,3}
    for (size_t i = 0; i < 4; i++) { // 4 : four registers
        if (curOp_ & (1 << i)) {
            context_.AddUpVsp(FOUR_BYTE_OFFSET);
        }
    }
    return true;
}

inline bool ArmExidx::Decode1100100nsssscccc()
{
    // 11001000 sssscccc: Pop VFP double precision registers D[16+ssss]-D[16+ssss+cccc] by VPUSH
    // 11001001 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by VPUSH
    if (!GetOpCode()) {
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
#endif