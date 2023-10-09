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
#ifndef UNWIND_X86_64_DEFINE_H
#define UNWIND_X86_64_DEFINE_H

#include <cinttypes>
#include <string>
#if defined(is_ohos) && is_ohos
#include <ucontext.h>
#endif
#include <vector>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 512
#define DWARF_PRESERVED_REGS_NUM 17

enum RegsEnumX86_64 : uint16_t {
    REG_X86_64_RAX = 0,
    REG_X86_64_RDX,
    REG_X86_64_RCX,
    REG_X86_64_RBX,
    REG_X86_64_RSI,
    REG_X86_64_RDI,
    REG_X86_64_RBP,
    REG_X86_64_RSP,
    REG_X86_64_R8,
    REG_X86_64_R9,
    REG_X86_64_R10,
    REG_X86_64_R11,
    REG_X86_64_R12,
    REG_X86_64_R13,
    REG_X86_64_R14,
    REG_X86_64_R15,
    REG_X86_64_RIP,
    REG_X86_64_LAST,

    REG_SP = REG_X86_64_RSP,
    REG_PC = REG_X86_64_RIP,
    REG_EH = REG_X86_64_RAX,
    REG_LAST = REG_X86_64_LAST,
};

struct RegsUserX86_64 {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t orig_rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t eflags;
    uint64_t rsp;
    uint64_t ss;
    uint64_t fs_base;
    uint64_t gs_base;
    uint64_t ds;
    uint64_t es;
    uint64_t fs;
    uint64_t gs;
};

static const std::vector<int> QUT_REGS {
    // empty
};
#if defined(is_ohos) && is_ohos
typedef ucontext_t UnwindUContext_t;
#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif
