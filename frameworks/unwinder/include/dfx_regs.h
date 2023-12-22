/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef DFX_REGS_H
#define DFX_REGS_H

#include <cstdint>
#include <string>
#include <memory>
#include <sys/types.h>
#include <ucontext.h>
#include <vector>
#include "dfx_define.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
class DfxRegs {
public:
    DfxRegs() = default;
    virtual ~DfxRegs() {};
    static std::shared_ptr<DfxRegs> Create(int mode = UnwindMode::DWARF_UNWIND);
    static std::shared_ptr<DfxRegs> CreateFromContext(const ucontext_t &context);

    inline uintptr_t& operator[](size_t reg) { return regsData_[reg]; }

    void* RawData() { return regsData_.data(); }
    std::vector<uintptr_t> GetRegsData() const;
    void SetRegsData(const std::vector<uintptr_t>& regs);

    virtual std::string PrintRegs() const = 0;
    virtual void GetFramePointerMiniRegs(void *regs) = 0;
    virtual void GetQuickenMiniRegs(void *regs) = 0;

    std::string GetSpecialRegisterName(uintptr_t val) const;
protected:
    std::string PrintSpecialRegs() const;
public:
    uintptr_t fp_ {0};
    uintptr_t pc_ {0};
    uintptr_t sp_ {0};
    uintptr_t lr_ {0};
protected:
    std::vector<uintptr_t> regsData_ {};
};

class DfxRegsArm : public DfxRegs {
public:
    explicit DfxRegsArm(const ucontext_t &context);
    DfxRegsArm() = default;
    ~DfxRegsArm() override {};
    std::string PrintRegs() const override;
    // Get 4 registers(r7/r11/sp/pc)
    inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs) override
    {
#if defined(__arm__)
        asm volatile(
        ".align 2\n"
        "bx pc\n"
        "nop\n"
        ".code 32\n"
        "stmia %[base], {r7, r11}\n"
        "add %[base], #8\n"
        "mov r1, r13\n"
        "mov r2, r15\n"
        "stmia %[base], {r1, r2}\n"
        "orr %[base], pc, #1\n"
        "bx %[base]\n"
        : [base] "+r"(regs)
        :
        : "r1", "r2", "memory");
#endif
    }

    // Get 7 registers [r4, r7, r10, r11, sp, pc, unset].
    inline AT_ALWAYS_INLINE void GetQuickenMiniRegs(void *regs) override
    {
#if defined(__arm__)
        asm volatile(
        ".align 2\n"
        "bx pc\n"
        "nop\n"
        ".code 32\n"
        "stmia %[base], {r4, r7, r10, r11}\n"
        "add %[base], #16\n"
        "mov r1, r13\n"
        "mov r2, r15\n"
        "stmia %[base], {r1, r2}\n"
        "orr %[base], pc, #1\n"
        "bx %[base]\n"
        : [base] "+r"(regs)
        :
        : "r1", "r2", "memory");
#endif
    }
};

class DfxRegsArm64 : public DfxRegs {
public:
    explicit DfxRegsArm64(const ucontext_t &context);
    DfxRegsArm64() = default;
    ~DfxRegsArm64() override {};
    std::string PrintRegs() const override;
    // Get 4 registers from x29 to x32.
    inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs) override
    {
#if defined(__aarch64__)
        asm volatile(
        "1:\n"
        "stp x29, x30, [%[base], #0]\n"
        "mov x12, sp\n"
        "adr x13, 1b\n"
        "stp x12, x13, [%[base], #16]\n"
        : [base] "+r"(regs)
        :
        : "x12", "x13", "memory");
#endif
    }

    // Get 7 registers with [unuse, unset, x28, x29, sp, pc, unset].
    inline AT_ALWAYS_INLINE void GetQuickenMiniRegs(void *regs) override
    {
#if defined(__aarch64__)
        asm volatile(
        "1:\n"
        "stp x28, x29, [%[base], #16]\n"
        "mov x12, sp\n"
        "adr x13, 1b\n"
        "stp x12, x13, [%[base], #32]\n"
        : [base] "+r"(regs)
        :
        : "x12", "x13", "memory");
#endif
    }
};

class DfxRegsRiscv64 : public DfxRegs {
public:
    explicit DfxRegsRiscv64(const ucontext_t &context);
    DfxRegsRiscv64() = default;
    ~DfxRegsRiscv64() override {};
    std::string PrintRegs() const override;
    // Get 4 registers from x29 to x32.
    inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs) override
    {
#if defined(__riscv) && __riscv_xlen == 64
//future work
        asm volatile(
        "ret");
#endif
    }

    // Get 7 registers with [unuse, unset, x28, x29, sp, pc, unset].
    inline AT_ALWAYS_INLINE void GetQuickenMiniRegs(void *regs) override
    {
#if defined(__riscv) && __riscv_xlen == 64
//future work
        asm volatile(
        "ret");
#endif
    }
};

class DfxRegsX86_64 : public DfxRegs {
public:
    explicit DfxRegsX86_64(const ucontext_t &context);
    DfxRegsX86_64() = default;
    ~DfxRegsX86_64() override {};
    std::string PrintRegs() const override;
    inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs) override {};
    inline AT_ALWAYS_INLINE void GetQuickenMiniRegs(void *regs) override {};
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
