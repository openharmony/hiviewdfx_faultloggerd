/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#ifndef DFX_REGS_GET_H
#define DFX_REGS_GET_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include "dfx_define.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {

#if defined(__arm__)

inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
    asm volatile(
        ".align 2\n"
        "bx pc\n"
        "nop\n"
        ".code 32\n"
        "stmia %[base], {r0-r12}\n"
        "add %[base], #52\n"
        "mov r1, r13\n"
        "mov r2, r14\n"
        "mov r3, r15\n"
        "stmia %[base], {r1-r3}\n"
        "orr %[base], pc, #1\n"
        "bx %[base]\n"
        : [base] "+r"(regs)
        :
        : "memory");
}

// Only get 4 registers(r7/r11/sp/pc)
inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs, size_t size)
{
    if (size < FP_MINI_REGS_SIZE) {
        return;
    }
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
}

// Fill regs[7] with [r4, r7, r10, r11, sp, pc, unset].
inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
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
}

#elif defined(__aarch64__)

inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
    asm volatile(
        "1:\n"
        "stp x0, x1, [%[base], #0]\n"
        "stp x2, x3, [%[base], #16]\n"
        "stp x4, x5, [%[base], #32]\n"
        "stp x6, x7, [%[base], #48]\n"
        "stp x8, x9, [%[base], #64]\n"
        "stp x10, x11, [%[base], #80]\n"
        "stp x12, x13, [%[base], #96]\n"
        "stp x14, x15, [%[base], #112]\n"
        "stp x16, x17, [%[base], #128]\n"
        "stp x18, x19, [%[base], #144]\n"
        "stp x20, x21, [%[base], #160]\n"
        "stp x22, x23, [%[base], #176]\n"
        "stp x24, x25, [%[base], #192]\n"
        "stp x26, x27, [%[base], #208]\n"
        "stp x28, x29, [%[base], #224]\n"
        "str x30, [%[base], #240]\n"
        "mov x12, sp\n"
        "adr x13, 1b\n"
        "stp x12, x13, [%[base], #248]\n"
        : [base] "+r"(regs)
        :
        : "x12", "x13", "memory");
}

// Only get 4 registers from x29 to x32.
inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs, size_t size)
{
    if (size < FP_MINI_REGS_SIZE) {
        return;
    }
    asm volatile(
    "1:\n"
    "stp x29, x30, [%[base], #0]\n"
    "mov x12, sp\n"
    "adr x13, 1b\n"
    "stp x12, x13, [%[base], #16]\n"
    : [base] "+r"(regs)
    :
    : "x12", "x13", "memory");
}

// Fill regs[7] with [unuse, unset, x28, x29, sp, pc, unset].
inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
    asm volatile(
    "1:\n"
    "stp x28, x29, [%[base], #16]\n"
    "mov x12, sp\n"
    "adr x13, 1b\n"
    "stp x12, x13, [%[base], #32]\n"
    : [base] "+r"(regs)
    :
    : "x12", "x13", "memory");
}

#elif defined(__x86_64__)
extern "C" int _x86_64_getcontext(void* regs);
inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
    _x86_64_getcontext(regs);
}

inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs, size_t size)
{
}

inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
}

#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64

//future work
inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
}

inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs, size_t size)
{
}

inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
}

#elif defined(__loongarch_lp64)

inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
    asm volatile(
        "st.d $r0, %[base], 0\n"
        "st.d $r1, %[base], 8\n"
        "st.d $r2, %[base], 16\n"
        "st.d $r3, %[base], 24\n"
        "st.d $r4, %[base], 32\n"
        "st.d $r5, %[base], 40\n"
        "st.d $r6, %[base], 48\n"
        "st.d $r7, %[base], 56\n"
        "st.d $r8, %[base], 64\n"
        "st.d $r9, %[base], 72\n"
        "st.d $r10, %[base], 80\n"
        "st.d $r11, %[base], 88\n"
        "st.d $r12, %[base], 96\n"
        "st.d $r13, %[base], 104\n"
        "st.d $r14, %[base], 112\n"
        "st.d $r15, %[base], 120\n"
        "st.d $r16, %[base], 128\n"
        "st.d $r17, %[base], 136\n"
        "st.d $r18, %[base], 144\n"
        "st.d $r19, %[base], 152\n"
        "st.d $r20, %[base], 160\n"
        "st.d $r21, %[base], 168\n"
        "st.d $r22, %[base], 176\n"
        "st.d $r23, %[base], 184\n"
        "st.d $r24, %[base], 192\n"
        "st.d $r25, %[base], 200\n"
        "st.d $r26, %[base], 208\n"
        "st.d $r27, %[base], 216\n"
        "st.d $r28, %[base], 224\n"
        "st.d $r29, %[base], 232\n"
        "st.d $r30, %[base], 240\n"
        "st.d $r31, %[base], 248\n"
        "pcaddi $t0, -32\n"
        "st.d $t0, %[base], 256\n"
        : [base] "+r"(regs)
        :
        : "$t0", "memory");
}

inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs)
{
    asm volatile(
        "st.d $fp, %[base], 0\n"
        "st.d $ra, %[base], 8\n"
        "st.d $sp, %[base], 16\n"
        "pcaddi $t0, -3\n"
        "st.d $t0, %[base], 24\n"
        : [base] "+r"(regs)
        :
        : "$t0", "memory");
}

inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
    asm volatile(
        "st.d $fp, %[base], 24\n"
        "st.d $sp, %[base], 32\n"
        "pcaddi $t0, -2\n"
        "st.d $t0, %[base], 40\n"
        "st.d $ra, %[base], 48\n"
        : [base] "+r"(regs)
        :
        : "$t0", "memory");
}

#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif
