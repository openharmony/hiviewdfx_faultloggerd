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
#ifndef DFX_REGS_GET_H
#define DFX_REGS_GET_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include "dfx_define.h"

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
inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs)
{
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
inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs)
{
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

inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
}

inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs)
{
}

inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
}

#elif defined(__riscv) && __riscv_xlen == 64

//future work
inline AT_ALWAYS_INLINE void GetLocalRegs(void* regs)
{
}

inline AT_ALWAYS_INLINE void GetFramePointerMiniRegs(void *regs)
{
}

inline AT_ALWAYS_INLINE void GetQuickenMiniRegsAsm(void *regs)
{
}

#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif
