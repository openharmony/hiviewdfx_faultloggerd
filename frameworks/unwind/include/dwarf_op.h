
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
#include <deque>
#include <type_traits>
#include <memory>
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "dwarf_cfa_instructions.h"

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

    AddressType Eval(DfxRegs& regs, AddressType initStackValue, AddressType startPtr);

private:
    bool Decode(DfxRegs& regs, uintptr_t& addr);

    inline void StackReset(AddressType initialStackValue)
    {
        stack_.clear();
        stack_.push_front(initialStackValue);
    };
    inline void StackPush(AddressType value)
    {
        stack_.push_front(value);
    }
    inline AddressType StackPop()
    {
        AddressType value = stack_.front();
        stack_.pop_front();
        return value;
    }
    inline AddressType StackAt(size_t index) { return stack_[index]; }
    inline size_t StackSize() { return stack_.size(); }

    // DW_OP_addr DW_OP_constXs DW_OP_constXu
    template <typename T>
    inline void OpPush(T value) {
        StackPush(static_cast<AddressType>(value));
    };

    // DW_OP_deref
    inline void OpDeref() {
        AddressType addr = StackPop();
        AddressType value = memory_->Read<uintptr_t>(addr);
        StackPush(value);
    };

    // DW_OP_deref_size
    inline void OpDerefSize(AddressType& exprPtr) {
        AddressType addr = StackPop();
        AddressType value = 0;
        uint8_t operand = memory_->Read<uint8_t>(exprPtr, true);
        switch (operand) {
            case 1:  // 1 Byte
                value = memory_->Read<uint8_t>(addr);
                break;
            case 2:  // 2 Byte
                value = memory_->Read<uint16_t>(addr);
                break;
            case 4:  // 4 Byte
                value = memory_->Read<uint32_t>(addr);
                break;
            case 8:  // 8 Byte
                value = static_cast<UnsignedType>(memory_->Read<uint64_t>(addr));
                break;
        }
        StackPush(static_cast<UnsignedType>(value));
    };

    // DW_OP_dup
    inline void OpDup() {
        AddressType value = StackAt(0);
        StackPush(value);
    };

    // DW_OP_drop
    inline void OpDrop() {
        StackPop();
    };

    // DW_OP_over
    inline void OpOver() {
        AddressType value = StackAt(1);
        StackPush(value);
    };

    // DW_OP_pick
    inline void OpPick(AddressType& exprPtr) {
        uint32_t reg = memory_->Read<uint8_t>(exprPtr, true);
        if (reg > StackSize()) {
            return;
        }
        AddressType value = StackAt(reg);
        StackPush(value);
    };

    // DW_OP_swap
    inline void OpSwap() {
        AddressType oldValue = stack_[0];
        stack_[0] = stack_[1];
        stack_[1] = oldValue;
    }

    // DW_OP_rot
    inline void OpRot() {
        AddressType top = stack_[0];
        stack_[0] = stack_[1];
        stack_[1] = stack_[2];
        stack_[2] = top;
    }

    // DW_OP_abs
    inline void OpAbs() {
        SignedType signedValue = static_cast<SignedType>(stack_[0]);
        if (signedValue < 0) {
            signedValue = -signedValue;
        }
        stack_[0] = static_cast<AddressType>(signedValue);
    };

    // DW_OP_and
    inline void OpAnd() {
        AddressType top = StackPop();
        stack_[0] &= top;
    };

    // DW_OP_div
    inline void OpDiv() {
        AddressType top = StackPop();
        if (top == 0) {
            return;
        }
        SignedType signedDivisor = static_cast<SignedType>(top);
        SignedType signedDividend = static_cast<SignedType>(stack_[0]);
        stack_[0] = static_cast<AddressType>(signedDividend / signedDivisor);
    };

    // DW_OP_minus
    inline void OpMinus() {
        AddressType top = StackPop();
        stack_[0] -= top;
    };

    // DW_OP_mod
    inline void OpMod() {
        AddressType top = StackPop();
        if (top == 0) {
            return;
        }
        stack_[0] %= top;
    };

    inline void OpMul() {
        AddressType top = StackPop();
        stack_[0] *= top;
    };

    inline void OpNeg() {
        SignedType signedValue = static_cast<SignedType>(stack_[0]);
        stack_[0] = static_cast<AddressType>(-signedValue);
    };

    inline void OpNot() {
        stack_[0] = ~stack_[0];
    };

    inline void OpOr() {
        AddressType top = StackPop();
        stack_[0] |= top;
    };

    inline void OpPlus() {
        AddressType top = StackPop();
        stack_[0] += top;
    };

    inline void OpPlusULEBConst(AddressType& exprPtr) {
        stack_[0] += memory_->ReadUleb128(exprPtr);
    };

    inline void OpShl() {
        AddressType top = StackPop();
        stack_[0] <<= top;
    };

    inline void OpShr() {
        AddressType top = StackPop();
        stack_[0] >>= top;
    };

    inline void OpShra() {
        AddressType top = StackPop();
        SignedType signedValue = static_cast<SignedType>(stack_[0]) >> top;
        stack_[0] = static_cast<AddressType>(signedValue);
    };

    inline void OpXor() {
        AddressType top = StackPop();
        stack_[0] ^= top;
    };

    inline void OpSkip(AddressType& exprPtr) {
        auto offset = memory_->Read<int16_t>(exprPtr, true);
        exprPtr = static_cast<AddressType>(exprPtr + offset);
    };

    // DW_OP_bra
    inline void OpBra(AddressType& exprPtr) {
        AddressType top = StackPop();
        int16_t offset = memory_->Read<int16_t>(exprPtr, true);
        if (top != 0) {
            exprPtr =exprPtr + offset;
        } else {
            exprPtr = exprPtr - offset;
        }
    };

    inline void OpEQ() {
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] == top) ? 1 : 0);
    };

    inline void OpGE() {
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] >= top) ? 1 : 0);
    };

    inline void OpGT() {
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] > top) ? 1 : 0);
    };

    inline void OpLE() {
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] <= top) ? 1 : 0);
    };

    inline void OpLT() {
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] < top) ? 1 : 0);
    };

    inline void OpNE() {
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] != top) ? 1 : 0);
    };

    // DW_OP_litXX
    inline void OpLit(uint8_t opcode) {
        stack_.push_front(opcode - DW_OP_lit0);
    };

    // DW_OP_regXX
    inline void OpReg(uint8_t opcode, DfxRegs& regs) {
        auto reg = static_cast<UnsignedType>(opcode - DW_OP_reg0);
        stack_.push_front(regs[reg]);
    };

    // DW_OP_regx
    inline void OpRegx(AddressType& exprPtr, DfxRegs& regs) {
        auto reg = static_cast<uint32_t>(memory_->ReadUleb128(exprPtr));
        stack_.push_front(regs[reg]);
    };

    inline void OpBReg(uint8_t opcode, AddressType& exprPtr, DfxRegs& regs) {
        auto reg = static_cast<uint32_t>(opcode - DW_OP_breg0);
        auto value = static_cast<SignedType>(memory_->ReadSleb128(exprPtr));
        value += static_cast<SignedType>(regs[reg]);
        stack_.push_front(value);
    };

    inline void OpBRegx(AddressType& exprPtr, DfxRegs& regs) {
        auto reg = static_cast<uint32_t>(memory_->ReadUleb128(exprPtr));
        auto value = static_cast<SignedType>(memory_->ReadSleb128(exprPtr));
        value += static_cast<SignedType>(regs[reg]);
        stack_.push_front(value);
    };

    inline void OpNop(uint8_t opcode) {
        // log un-implemmented operate codes
    };

private:
    std::shared_ptr<DfxMemory> memory_;
    std::deque<AddressType> stack_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif