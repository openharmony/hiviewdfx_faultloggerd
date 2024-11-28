/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#include <securec.h>

#include "dfx_define.h"
#include "dfx_regs.h"
#include "dfx_regs_qut.h"
#include "dfx_log.h"
#include "dfx_instr_statistic.h"
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

static const uintptr_t FOUR_BYTE_OFFSET = 4;
static const int TWO_BIT_OFFSET = 2;
static const int FOUR_BIT_OFFSET = 4;
static const int SEVEN_BIT_OFFSET = 7;
static const int EIGHT_BIT_OFFSET = 8;
static const int SIXTEEN_BIT_OFFSET = 16;
static const int TWENTY_FOUR_BIT_OFFSET = 24;
static const int TWENTY_EIGHT_BIT_OFFSET = 28;
}

void ExidxContext::Reset(size_t size)
{
    vsp = 0;
    transformedBits = 0;
    if (size != 0) {
        regs.resize(size);
    }
    for (size_t i = 0; i < regs.size(); ++i) {
        regs[i] = 0;
    }
}

void ExidxContext::Transform(uint32_t reg)
{
    DFXLOGU("Transform reg: %{public}d", reg);
    transformedBits = transformedBits | (1 << reg);
}

bool ExidxContext::IsTransformed(uint32_t reg)
{
    if (transformedBits & (1 << reg)) {
        return true;
    }
    return false;
}

void ExidxContext::AddUpVsp(int32_t imm)
{
    DFXLOGU("AddUpVsp imm: %{public}d", imm);
    vsp += imm;

    auto qutRegs = DfxRegsQut::GetQutRegs();
    for (size_t i = 0; i < qutRegs.size(); i++) {
        uint32_t reg = static_cast<uint32_t>(qutRegs[i]);
        if (IsTransformed(reg)) {
            regs[i] += imm;
        }
    }
}

ArmExidx::ArmExidx(std::shared_ptr<DfxMemory> memory) : memory_(memory)
{
    (void)memset_s(&lastErrorData_, sizeof(UnwindErrorData), 0, sizeof(UnwindErrorData));
    context_.Reset(DfxRegsQut::GetQutRegsSize());
}

inline void ArmExidx::FlushInstr()
{
    if (rsState_->cfaReg == 0) {
        rsState_->cfaReg = REG_SP;
    }
    rsState_->cfaRegOffset = 0;
    if (context_.vsp != 0) {
        if (__builtin_expect(!((context_.vsp & 0x3) == 0), false)) {
            DFXLOGE("Check failed: context_.vsp & 0x3) == 0");
        }
        rsState_->cfaRegOffset = context_.vsp;
    }
    DFXLOGU("rsState cfaReg: %{public}d, cfaRegOffset: %{public}d", rsState_->cfaReg, rsState_->cfaRegOffset);

    auto qutRegs = DfxRegsQut::GetQutRegs();
    for (size_t i = 0; i < qutRegs.size(); i++) {
        uint32_t reg = static_cast<uint32_t>(qutRegs[i]);
        if (context_.IsTransformed(reg)) {
            if (__builtin_expect(!((context_.regs[i] & 0x3) == 0), false)) {
                DFXLOGE("Check failed: context_.regs[%{public}zu] & 0x3) == 0", i);
            }
            rsState_->locs[i].type = REG_LOC_MEM_OFFSET;
            rsState_->locs[i].val = -context_.regs[i];
            DFXLOGU("rsState reg: %{public}d, locs[%{public}d].val: %{public}d", reg, i, rsState_->locs[i].val);
        }
    }

    if (!isPcSet_) {
        rsState_->returnAddressRegister = REG_LR;
    } else {
        rsState_->returnAddressRegister = REG_PC;
    }

    context_.Reset();
}

inline void ArmExidx::LogRawData()
{
    std::string logStr("Raw Data:");
    for (const uint8_t data : ops_) {
        logStr += StringPrintf(" 0x%02x", data);
    }
    DFXLOGU("%{public}s", logStr.c_str());
}

bool ArmExidx::SearchEntry(uintptr_t pc, struct UnwindTableInfo uti, struct UnwindEntryInfo& uei)
{
    uintptr_t tableLen = uti.tableLen / ARM_EXIDX_TABLE_SIZE;
    uintptr_t tableData = uti.tableData;
    DFXLOGU("SearchEntry pc: %{public}p tableData: %{public}p, tableLen: %{public}u",
        (void*)pc, (void*)tableData, (uint32_t)tableLen);
    if (tableLen == 0) {
        lastErrorData_.SetCode(UNW_ERROR_NO_UNWIND_INFO);
        return false;
    }

    // do binary search
    uintptr_t entry = 0;
    uintptr_t low = 0;
    uintptr_t high = tableLen;
    while (low < high) {
        uintptr_t cur = (low + high) / 2; // 2 : binary search divided parameter
        uintptr_t ptr = tableData + cur * ARM_EXIDX_TABLE_SIZE;
        uintptr_t addr = 0;
        if (!memory_->ReadPrel31(ptr, &addr)) {
            lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_ILLEGAL_VALUE);
            return false;
        }

        if (pc == addr) {
            entry = ptr;
            break;
        }
        if (pc < addr) {
            high = cur;
        } else {
            low = cur + 1;
        }
    }
    if (entry == 0) {
        if (high != 0) {
            entry = tableData + (high - 1) * ARM_EXIDX_TABLE_SIZE;
        } else {
            lastErrorData_.SetCode(UNW_ERROR_NO_UNWIND_INFO);
            return false;
        }
    }

    uei.unwindInfoSize = ARM_EXIDX_TABLE_SIZE;
    uei.unwindInfo = (void *) entry;
    uei.format = UNW_INFO_FORMAT_ARM_EXIDX;
    return true;
}

bool ArmExidx::ExtractEntryData(uintptr_t entryOffset)
{
    DFXLOGU("Exidx entryOffset: %{public}llx", (uint64_t)entryOffset);
    ops_.clear();
    uint32_t data = 0;
    if (entryOffset & 1) {
        DFXLOGE("[%{public}d]: entryOffset: %{public}llx error.", __LINE__, (uint64_t)entryOffset);
        lastErrorData_.SetAddrAndCode(entryOffset, UNW_ERROR_INVALID_ALIGNMENT);
        return false;
    }

    entryOffset += FOUR_BYTE_OFFSET;
    if (!memory_->Read<uint32_t>(entryOffset, &data, false)) {
        DFXLOGE("[%{public}d]: entryOffset: %{public}llx error.", __LINE__, (uint64_t)entryOffset);
        lastErrorData_.SetAddrAndCode(entryOffset, UNW_ERROR_ILLEGAL_VALUE);
        return false;
    }

    if (data == ARM_EXIDX_CANT_UNWIND) {
        DFXLOGU("This is a CANT UNWIND entry, data: %{public}x.", data);
        lastErrorData_.SetAddrAndCode(entryOffset, UNW_ERROR_CANT_UNWIND);
        return false;
    } else if ((data & ARM_EXIDX_COMPACT) != 0) {
        if (((data >> TWENTY_FOUR_BIT_OFFSET) & 0x7f) != 0) {
            DFXLOGE("This is a non-zero index, this code doesn't support other formats.");
            lastErrorData_.SetCode(UNW_ERROR_INVALID_PERSONALITY);
            return false;
        }
        DFXLOGU("This is a compact table entry, data: %{public}x.", data);
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
#ifndef TEST_ARM_EXIDX
    if (!memory_->ReadPrel31(entryOffset, &extabAddr)) {
        lastErrorData_.SetAddrAndCode(entryOffset, UNW_ERROR_INVALID_MEMORY);
        return false;
    }
#else
    extabAddr = entryOffset + FOUR_BYTE_OFFSET;
#endif
    return ExtractEntryTab(extabAddr);
}

bool ArmExidx::ExtractEntryTabByPersonality(uintptr_t &tabOffset, uint32_t &data, uint8_t &tableCount)
{
    if ((data & ARM_EXIDX_COMPACT) == 0) {
        DFXLOGU("Arm generic personality, data: %{public}x.", data);
#ifndef TEST_ARM_EXIDX
        uintptr_t perRoutine;
        if (!memory_->ReadPrel31(tabOffset, &perRoutine)) {
            DFXLOGE("Arm Personality routine error");
            lastErrorData_.SetAddrAndCode(tabOffset, UNW_ERROR_INVALID_MEMORY);
            return false;
        }
#endif

        tabOffset += FOUR_BYTE_OFFSET;
        // Skip four bytes, because dont have unwind data to read
        if (!memory_->Read<uint32_t>(tabOffset, &data, false)) {
            lastErrorData_.SetAddrAndCode(tabOffset, UNW_ERROR_INVALID_MEMORY);
            return false;
        }
        tableCount = (data >> TWENTY_FOUR_BIT_OFFSET) & 0xff;
        ops_.push_back((data >> SIXTEEN_BIT_OFFSET) & 0xff);
        ops_.push_back((data >> EIGHT_BIT_OFFSET) & 0xff);
        ops_.push_back(data & 0xff);
        tabOffset += FOUR_BYTE_OFFSET;
    } else {
        DFXLOGU("Arm compact personality, data: %{public}x.", data);
        if ((data >> TWENTY_EIGHT_BIT_OFFSET) != 0x8) {
            DFXLOGE("incorrect Arm compact model, [31:28]bit must be 0x8(%{public}x)",
                data >> TWENTY_EIGHT_BIT_OFFSET);
            lastErrorData_.SetCode(UNW_ERROR_INVALID_PERSONALITY);
            return false;
        }
        uint8_t personality = (data >> TWENTY_FOUR_BIT_OFFSET) & 0x3;
        if (personality > 2) { // 2 : personality must be 0 1 2
            DFXLOGE("incorrect Arm compact personality(%{public}u)", personality);
            lastErrorData_.SetCode(UNW_ERROR_INVALID_PERSONALITY);
            return false;
        }
        // inline compact model, when personality is 0
        if (personality == 0) {
            ops_.push_back((data >> SIXTEEN_BIT_OFFSET) & 0xff);
        } else if (personality == 1 || personality == 2) { // 2 : personality equal to 2
            tableCount = (data >> SIXTEEN_BIT_OFFSET) & 0xff;
            tabOffset += FOUR_BYTE_OFFSET;
        }
        ops_.push_back((data >> EIGHT_BIT_OFFSET) & 0xff);
        ops_.push_back(data & 0xff);
    }
    return true;
}

bool ArmExidx::ExtractEntryTab(uintptr_t tabOffset)
{
    uint32_t data = 0;
    DFXLOGU("Exidx tabOffset: %{public}llx", (uint64_t)tabOffset);
    if (!memory_->Read<uint32_t>(tabOffset, &data, false)) {
        lastErrorData_.SetAddrAndCode(tabOffset, UNW_ERROR_INVALID_MEMORY);
        return false;
    }

    uint8_t tableCount = 0;
    if (!ExtractEntryTabByPersonality(tabOffset, data, tableCount)) {
        return false;
    }
    
    if (tableCount > 5) { // 5 : 5 operators
        lastErrorData_.SetCode(UNW_ERROR_NOT_SUPPORT);
        return false;
    }

    for (size_t i = 0; i < tableCount; i++) {
        if (!memory_->Read<uint32_t>(tabOffset, &data, false)) {
            return false;
        }
        tabOffset += FOUR_BYTE_OFFSET;
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
    DFXLOGU("curOp: %{public}llx", (uint64_t)curOp_);
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
    DFXLOGU("Exidx Decode Spare");
    lastErrorData_.SetCode(UNW_ERROR_ARM_EXIDX_SPARE);
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
                DFXLOGU("decodeTable[%{public}d].mask: %{public}02x", i, decodeTable[i].mask);
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
        DFXLOGE("10000000 00000000: Refuse to unwind!");
        lastErrorData_.SetCode(UNW_ERROR_CANT_UNWIND);
        return false;
    }

    registers <<= FOUR_BIT_OFFSET;
    DFXLOGU("1000iiii iiiiiiii: registers: %{public}02x)", registers);
    for (size_t reg = REG_ARM_R4; reg < REG_ARM_LAST; reg++) {
        if ((registers & (1 << reg))) {
            if (REG_PC == reg) {
                isPcSet_ = true;
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
        DFXLOGU("10011101 or 10011111: Reserved");
        lastErrorData_.SetCode(UNW_ERROR_RESERVED_VALUE);
        return false;
    }
    // 1001nnnn: Set vsp = r[nnnn]
    if ((bits == REG_ARM_R7) || (bits == REG_ARM_R11)) {
        DFXLOGU("1001nnnn: Set vsp = R%{public}d", bits);
        if (context_.transformedBits == 0) {
            // No register transformed, ignore vsp offset.
            context_.Reset();
        }
    } else {
        INSTR_STATISTIC(UnsupportedArmExidx, bits, curOp_);
        return false;
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
    DFXLOGU("%{public}s", msg.c_str());

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
    DFXLOGU("10110000: Finish");
    lastErrorData_.SetCode(UNW_ERROR_ARM_EXIDX_FINISH);
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
    DFXLOGU("10110001 0000iiii, registers: %{public}02x", registers);
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
    DFXLOGU("vsp = vsp + %{public}d", offset);
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