
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
#include "dfx_log.h"
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

protected:
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

    /* DW_OP_addr DW_OP_constXs DW_OP_constXu */
    template <typename T>
    inline void OpPush(T value)
    {
        StackPush(static_cast<AddressType>(value));
    };

    /* DW_OP_deref */
    inline void OpDeref()
    {
        LOGU("DW_OP_deref");
        auto addr = static_cast<uintptr_t>(StackPop());
        uintptr_t val;
        memory_->ReadUptr(addr, &val);
        StackPush(static_cast<AddressType>(val));
    };

    /* DW_OP_deref_size */
    void OpDerefSize(AddressType& exprPtr)
    {
        LOGU("DW_OP_deref_size");
        auto addr = static_cast<uintptr_t>(StackPop());
        AddressType value = 0;
        uint8_t operand;
        memory_->ReadU8(exprPtr, &operand, true);
        switch (operand) {
            case 1: { // 1 : read one byte length
                uint8_t u8;
                memory_->ReadU8(addr, &u8, true);
                value = static_cast<AddressType>(u8);
            }
                break;
            case 2: { // 2 : read two bytes length
                uint16_t u16;
                memory_->ReadU16(addr, &u16, true);
                value = static_cast<AddressType>(u16);
            }
                break;
            case 3: // 3 : read four bytes length
            case 4: { // 4 : read four bytes length
                uint32_t u32;
                memory_->ReadU32(addr, &u32, true);
                value = static_cast<AddressType>(u32);
            }
                break;
            case 5: // 5 : read eight bytes length
            case 6: // 6 : read eight bytes length
            case 7: // 7 : read eight bytes length
            case 8: { // 8 : read eight bytes length
                uint64_t u64;
                memory_->ReadU64(addr, &u64, true);
                value = static_cast<AddressType>(u64);
            }
                break;
            default:
                break;
        }
        StackPush(static_cast<UnsignedType>(value));
    };

    /* DW_OP_dup */
    inline void OpDup()
    {
        LOGU("DW_OP_dup");
        StackPush(StackAt(0));
    };

    /* DW_OP_drop */
    inline void OpDrop()
    {
        LOGU("DW_OP_drop");
        StackPop();
    };

    /* DW_OP_over */
    inline void OpOver()
    {
        LOGU("DW_OP_over");
        StackPush(StackAt(1));
    };

    /* DW_OP_pick */
    inline void OpPick(AddressType& exprPtr)
    {
        LOGU("DW_OP_pick");
        uint8_t reg;
        memory_->ReadU8(exprPtr, &reg, true);
        if (reg > StackSize()) {
            return;
        }
        AddressType value = StackAt(reg);
        StackPush(value);
    };

    /* DW_OP_swap */
    inline void OpSwap()
    {
        LOGU("DW_OP_swap");
        AddressType oldValue = stack_[0];
        stack_[0] = stack_[1];
        stack_[1] = oldValue;
    }

    /* DW_OP_rot */
    inline void OpRot()
    {
        LOGU("DW_OP_rot");
        AddressType top = stack_[0];
        stack_[0] = stack_[1];
        stack_[1] = stack_[2]; // 2:the index of the array
        stack_[2] = top; // 2:the index of the array
    }

    /* DW_OP_abs */
    inline void OpAbs()
    {
        LOGU("DW_OP_abs");
        SignedType signedValue = static_cast<SignedType>(stack_[0]);
        if (signedValue < 0) {
            signedValue = -signedValue;
        }
        stack_[0] = static_cast<AddressType>(signedValue);
    };

    /* DW_OP_and */
    inline void OpAnd()
    {
        LOGU("DW_OP_and");
        AddressType top = StackPop();
        stack_[0] &= top;
    };

    /* DW_OP_div */
    inline void OpDiv()
    {
        LOGU("DW_OP_div");
        AddressType top = StackPop();
        if (top == 0) {
            return;
        }
        SignedType signedDivisor = static_cast<SignedType>(top);
        SignedType signedDividend = static_cast<SignedType>(stack_[0]);
        stack_[0] = static_cast<AddressType>(signedDividend / signedDivisor);
    };

    /* DW_OP_minus */
    inline void OpMinus()
    {
        LOGU("DW_OP_minus");
        AddressType top = StackPop();
        stack_[0] -= top;
    };

    /* DW_OP_mod */
    inline void OpMod()
    {
        LOGU("DW_OP_mod");
        AddressType top = StackPop();
        if (top == 0) {
            return;
        }
        stack_[0] %= top;
    };

    /* DW_OP_mul */
    inline void OpMul()
    {
        LOGU("DW_OP_mul");
        AddressType top = StackPop();
        stack_[0] *= top;
    };

    inline void OpNeg()
    {
        LOGU("DW_OP_neg");
        SignedType signedValue = static_cast<SignedType>(stack_[0]);
        stack_[0] = static_cast<AddressType>(-signedValue);
    };

    inline void OpNot()
    {
        LOGU("DW_OP_not");
        stack_[0] = ~stack_[0];
    };

    inline void OpOr()
    {
        LOGU("DW_OP_or");
        AddressType top = StackPop();
        stack_[0] |= top;
    };

    inline void OpPlus()
    {
        LOGU("DW_OP_plus");
        AddressType top = StackPop();
        stack_[0] += top;
    };

    inline void OpPlusULEBConst(AddressType& exprPtr)
    {
        LOGU("DW_OP_plus_uconst");
        stack_[0] += memory_->ReadUleb128(exprPtr);
    };

    inline void OpShl()
    {
        LOGU("DW_OP_shl");
        AddressType top = StackPop();
        stack_[0] <<= top;
    };

    inline void OpShr()
    {
        LOGU("DW_OP_shr");
        AddressType top = StackPop();
        stack_[0] >>= top;
    };

    inline void OpShra()
    {
        LOGU("DW_OP_shra");
        AddressType top = StackPop();
        SignedType signedValue = static_cast<SignedType>(stack_[0]) >> top;
        stack_[0] = static_cast<AddressType>(signedValue);
    };

    inline void OpXor()
    {
        LOGU("DW_OP_xor");
        AddressType top = StackPop();
        stack_[0] ^= top;
    };

    inline void OpSkip(AddressType& exprPtr)
    {
        LOGU("DW_OP_skip");
        int16_t offset;
        memory_->ReadS16(exprPtr, &offset, true);
        exprPtr = static_cast<AddressType>(exprPtr + offset);
    };

    // DW_OP_bra
    inline void OpBra(AddressType& exprPtr)
    {
        LOGU("DW_OP_bra");
        AddressType top = StackPop();
        int16_t offset;
        memory_->ReadS16(exprPtr, &offset, true);
        if (top != 0) {
            exprPtr = exprPtr + offset;
        }
    };

    inline void OpEQ()
    {
        LOGU("DW_OP_eq");
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] == top) ? 1 : 0);
    };

    inline void OpGE()
    {
        LOGU("DW_OP_ge");
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] >= top) ? 1 : 0);
    };

    inline void OpGT()
    {
        LOGU("DW_OP_gt");
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] > top) ? 1 : 0);
    };

    inline void OpLE()
    {
        LOGU("DW_OP_le");
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] <= top) ? 1 : 0);
    };

    inline void OpLT()
    {
        LOGU("DW_OP_lt");
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] < top) ? 1 : 0);
    };

    inline void OpNE()
    {
        LOGU("DW_OP_ne");
        AddressType top = StackPop();
        stack_[0] = ((stack_[0] != top) ? 1 : 0);
    };

    // DW_OP_litXX
    inline void OpLit(uint8_t opcode)
    {
        LOGU("DW_OP_litXX");
        stack_.push_front(opcode - DW_OP_lit0);
    };

    // DW_OP_regXX
    inline void OpReg(uint8_t opcode, DfxRegs& regs)
    {
        LOGU("DW_OP_regXX");
        auto reg = static_cast<UnsignedType>(opcode - DW_OP_reg0);
        stack_.push_front(regs[reg]);
    };

    // DW_OP_regx
    inline void OpRegx(AddressType& exprPtr, DfxRegs& regs)
    {
        LOGU("DW_OP_regx");
        auto reg = static_cast<uint32_t>(memory_->ReadUleb128(exprPtr));
        stack_.push_front(regs[reg]);
    };

    inline void OpBReg(uint8_t opcode, AddressType& exprPtr, DfxRegs& regs)
    {
        LOGU("DW_OP_regx");
        auto reg = static_cast<uint32_t>(opcode - DW_OP_breg0);
        auto value = static_cast<SignedType>(memory_->ReadSleb128(exprPtr));
        value += static_cast<SignedType>(regs[reg]);
        stack_.push_front(value);
    };

    inline void OpBRegx(AddressType& exprPtr, DfxRegs& regs)
    {
        auto reg = static_cast<uint32_t>(memory_->ReadUleb128(exprPtr));
        auto value = static_cast<SignedType>(memory_->ReadSleb128(exprPtr));
        value += static_cast<SignedType>(regs[reg]);
        stack_.push_front(value);
    };

    inline void OpNop(uint8_t opcode)
    {
        // log un-implemmented operate codes
        LOGE("DWARF OpNop opcode: %x", opcode);
    };

protected:
    std::shared_ptr<DfxMemory> memory_;
    std::deque<AddressType> stack_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif