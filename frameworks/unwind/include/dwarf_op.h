
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
#ifndef DFX_DWARF_EXPRESSION_H
#define DFX_DWARF_EXPRESSION_H

#include <cinttypes>
#include <type_traits>
#include <memory>
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "dwarf_cfa_instructions.h"

#define MAX_DWARF_STACK_SZ 100

namespace OHOS {
namespace HiviewDFX {
// DWARF expressions describe how to compute a value or specify a location.
// They 2 are expressed in terms of DWARF operations that operate on a stack of values
template <typename AddressType>
class DwarfOp {
    using SignedType = typename std::make_signed<AddressType>::type;
    using UnsignedType = typename std::make_unsigned<AddressType>::type;

public:
    DwarfOp(std::shared_ptr<DfxMemory> memory) : memory_(memory) {};
    virtual ~DwarfOp() = default;

    AddressType Eval(DfxRegs& regs, AddressType initStackValue, AddressType start);

    bool StackTop(AddressType& value) {
        if (dwarfStackIndex_ > 0 && dwarfStackIndex_ < MAX_DWARF_STACK_SZ) {
            value = dwarfStack_[dwarfStackIndex_];
            return true;
        }
        return false;
    };

private:
    void StackReset(AddressType initialStackValue) {
        memset(dwarfStack_, 0, MAX_DWARF_STACK_SZ);
        dwarfStackIndex_ = 0;
        dwarfStack_[++dwarfStackIndex_] = initialStackValue;
    };

    // DW_OP_addr
    void OpAddr(AddressType& exprPtr) {
        auto value = memory_->Read<uintptr_t>(exprPtr, true);
        dwarfStack_[++dwarfStackIndex_] = value;
    };

    // DW_OP_deref
    void OpDeref() {
        // pop stack, dereference, push result
        auto addr = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[++dwarfStackIndex_] = memory_->Read<uintptr_t>(addr, true);;
    };

    // DW_OP_constXs DW_OP_constXu
    template <typename T>
    void OpPush(T value) {
        // push immediate sizeof(T）byte value
        dwarfStack_[++dwarfStackIndex_] = static_cast<AddressType>(value);
    };

    // DW_OP_dup
    void OpDup() {
        // push top of stack
        auto value = dwarfStack_[dwarfStackIndex_];
        dwarfStack_[++dwarfStackIndex_] = value;
    };

    // DW_OP_drop
    void OpDrop() {
        // pop
        dwarfStackIndex_--;
    };

    // DW_OP_over
    void OpOver() {
        auto value = dwarfStack_[dwarfStackIndex_ - 1];
        dwarfStack_[++dwarfStackIndex_] = value;
    };

    // DW_OP_pick
    void OpPick(AddressType& exprPtr) {
        uint32_t reg = memory_->Read<uint8_t>(exprPtr, true);
        auto value = dwarfStack_[-reg];
        dwarfStack_[++dwarfStackIndex_] = value;
    };

    // DW_OP_swap
    void OpSwap() {
        auto value = dwarfStack_[dwarfStackIndex_];
        dwarfStack_[dwarfStackIndex_] = dwarfStack_[dwarfStackIndex_ - 1];
        dwarfStack_[dwarfStackIndex_ - 1] = value;
    }

    // DW_OP_rot
    void OpRot() {
        auto value = dwarfStack_[dwarfStackIndex_];
        dwarfStack_[dwarfStackIndex_] = dwarfStack_[dwarfStackIndex_ - 1];
        dwarfStack_[dwarfStackIndex_ - 1] = dwarfStack_[dwarfStackIndex_ - 2];
        dwarfStack_[dwarfStackIndex_ - 2] = value;
    }

    // DW_OP_xderef
    void OpXderef() {
        auto addr = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = memory_->Read<UnsignedType>(addr);
    };

    // DW_OP_abs
    void OpAbs() {
        auto value = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_]);
        if (value < 0) {
            dwarfStack_[dwarfStackIndex_] = static_cast<UnsignedType>(-value);
        }
    };

    // DW_OP_and
    void OpAnd() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] &= value;
    };

    // DW_OP_div
    void OpDiv() {
        auto value1 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_--]);
        auto value2 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_]);
        dwarfStack_[dwarfStackIndex_] = value2 / value1;
    };

    // DW_OP_minus
    void OpMinus() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = dwarfStack_[dwarfStackIndex_] - value;
    };

    // DW_OP_mod
    void OpMod() {
        auto value1 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_--]);
        auto value2 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_]);
        dwarfStack_[dwarfStackIndex_] = static_cast<AddressType>(value2 % value1);
    };

    void OpMul() {
        auto value1 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_--]);
        auto value2 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_]);
        dwarfStack_[dwarfStackIndex_] = static_cast<AddressType>(value2 * value1);
    };

    void OpNeg() {
        dwarfStack_[dwarfStackIndex_] = 0 - dwarfStack_[dwarfStackIndex_];
    };

    void OpNot() {
        auto value1 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_]);
        dwarfStack_[dwarfStackIndex_] = static_cast<AddressType>(~value1);
    };

    void OpOr() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] |= value;
    };

    void OpPlus() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] += value;
    };

    void OpPlusULEBConst(AddressType& exprPtr) {
        dwarfStack_[dwarfStackIndex_] += memory_->ReadUleb128(exprPtr);
    };

    void OpShl() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = dwarfStack_[dwarfStackIndex_] << value;
    };

    void OpShr() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = dwarfStack_[dwarfStackIndex_] >> value;
    };

    void OpShra() {
        auto value1 = dwarfStack_[dwarfStackIndex_--];
        auto value2 = static_cast<SignedType>(dwarfStack_[dwarfStackIndex_]);
        dwarfStack_[dwarfStackIndex_] = static_cast<UnsignedType>(value2 >> value1);
    };

    void OpXor() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] ^= value;
    };

    void OpSkip(AddressType& exprPtr) {
        auto value1 = memory_->Read<int16_t>(exprPtr, true);
        exprPtr = static_cast<AddressType>(static_cast<SignedType>(exprPtr) + value1);
    };

    // DW_OP_bra
    void OpBra(AddressType& exprPtr) {
        auto value = memory_->Read<int16_t>(exprPtr, true);
        if (dwarfStack_[dwarfStackIndex_--] != 0) {
            exprPtr = static_cast<UnsignedType>(static_cast<SignedType>(exprPtr) + value);
        }
    };

    void OpEQ() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = (dwarfStack_[dwarfStackIndex_] == value);
    };

    void OpGE() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = (dwarfStack_[dwarfStackIndex_] >= value);
    };

    void OpGT() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = (dwarfStack_[dwarfStackIndex_] > value);
    };

    void OpLE() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = (dwarfStack_[dwarfStackIndex_] <= value);
    };

    void OpLT() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = (dwarfStack_[dwarfStackIndex_] < value);
    };

    void OpNE() {
        auto value = dwarfStack_[dwarfStackIndex_--];
        dwarfStack_[dwarfStackIndex_] = (dwarfStack_[dwarfStackIndex_] != value);
    };

    // DW_OP_litXX
    void OpLit(uint8_t opcode) {
        auto value = static_cast<UnsignedType>(opcode - DW_OP_lit0);
        dwarfStack_[++dwarfStackIndex_] = value;
    };

    // DW_OP_regXX
    void Opreg(uint8_t opcode, DfxRegs& regs) {
        auto regValue = static_cast<UnsignedType>(opcode - DW_OP_reg0);
        dwarfStack_[++dwarfStackIndex_] = regs[regValue];  // todo, reg wrapper
    };

    // DW_OP_regx
    void OpRegx(AddressType& exprPtr, DfxRegs& regs) {
        auto regValue = static_cast<uint32_t>(memory_->ReadUleb128(exprPtr));
        dwarfStack_[++dwarfStackIndex_] = regs[regValue];  // todo, reg wrapper
    };

    void OpBReg(uint8_t opcode, AddressType& exprPtr, DfxRegs& regs) {
        auto regValue = static_cast<uint32_t>(opcode - DW_OP_breg0);
        auto value = static_cast<SignedType>(memory_->ReadSleb128(exprPtr));
        value += static_cast<SignedType>(regs[regValue]);  // todo, reg wrapper
        dwarfStack_[++dwarfStackIndex_] = static_cast<UnsignedType>(value);
    };

    void OpBRegx(AddressType& exprPtr, DfxRegs& regs) {
        auto regValue = static_cast<uint32_t>(memory_->ReadUleb128(exprPtr));
        auto value = static_cast<SignedType>(memory_->ReadSleb128(exprPtr));
        value += static_cast<SignedType>(regs[regValue]);  // todo, reg wrapper
        dwarfStack_[++dwarfStackIndex_] = static_cast<UnsignedType>(value);
    };

    void OpDerefSize(AddressType& exprPtr) {
        auto value = dwarfStack_[dwarfStackIndex_--];
        uint8_t operand = memory_->Read<uint8_t>(value);
        switch (operand) {
            case 1:  // 1 Byte
                value = memory_->Read<uint8_t>(value);
                break;
            case 2:  // 2 Byte
                value = memory_->Read<uint16_t>(value);
                break;
            case 4:  // 4 Byte
                value = memory_->Read<uint32_t>(value);
                break;
            case 8:  // 8 Byte
                value = static_cast<UnsignedType>(memory_->Read<uint64_t>(value));
                break;
        }
        dwarfStack_[++dwarfStackIndex_] = static_cast<UnsignedType>(value);
    };

    void OpNop(uint8_t opcode){
        // log un-implemmented operate codes
    };

private:
    std::shared_ptr<DfxMemory> memory_;
    AddressType exprStart_;
    AddressType exprEnd_;
    AddressType dwarfStack_[MAX_DWARF_STACK_SZ];
    uint32_t dwarfStackIndex_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif