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
#ifndef UNWINDER_X86_DEFINE_H
#define UNWINDER_X86_DEFINE_H

#include <cinttypes>
#include <string>
#include <ucontext.h>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 512
#define UNWIND_CURSOR_LEN 127
#define DWARF_PRESERVED_REGS_NUM 17

enum RegsEnumX86 : uint16_t {
    REG_X86_EAX = 0,
    REG_X86_EDX,
    REG_X86_ECX,
    REG_X86_EBX,
    REG_X86_ESI,
    REG_X86_EDI,
    REG_X86_EBP,
    REG_X86_ESP,
    REG_X86_EIP,
    REG_X86_EFLAGS,
    REG_X86_GS,
    REG_X86_FS,
    REG_X86_ES,
    REG_X86_DS,
    REG_X86_CS,
    REG_X86_TSS,
    REG_X86_LAST,

    REG_SP = REG_X86_ESP,
    REG_PC = REG_X86_EIP,
    REG_EH = REG_X86_EAX,
    REG_LAST = REG_X86_LAST,
};

struct RegsUserX86 {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint32_t xds;
    uint32_t xes;
    uint32_t xfs;
    uint32_t xgs;
    uint32_t orig_eax;
    uint32_t eip;
    uint32_t xcs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t xss;
};

typedef ucontext_t UContext_t;

struct UnwindFrameInfo {
    /* no x86-specific fast trace */
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
