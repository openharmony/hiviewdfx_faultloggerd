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
#ifndef UNWINDER_DEFINE_H
#define UNWINDER_DEFINE_H

#include <cinttypes>
#include <string>
#if defined(__arm__)
#include "unwind_arm_define.h"
#elif defined(__aarch64__)
#include "unwind_arm64_define.h"
#elif defined(__i386__)
#include "unwind_x86_define.h"
#elif defined(__x86_64__)
#include "unwind_x86_64_define.h"
#else
#error "Unsupported architecture"
#endif

namespace OHOS {
namespace HiviewDFX {
#define FP_MINI_REGS_SIZE 4
#define QUT_MINI_REGS_SIZE 7

static const int FRAME_MAX_SIZE = 64;
static const int REGS_MAX_SIZE = 64;

/**
 * @brief chip architecture
 */
enum ArchType : uint8_t {
    ARCH_UNKNOWN = 0,
    ARCH_ARM,
    ARCH_ARM64,
    ARCH_X86,
    ARCH_X86_64,
};

enum UnwindFrameType
{
    UNWIND_FRAME_ALIGNED = -3,       /* x86_64: frame stack pointer aligned */
    UNWIND_FRAME_SYSCALL = -3,      /* arm32: r7 saved in r12, sp offset zero */
    UNWIND_FRAME_STANDARD = -2,     /* regular r7, sp +/- offset */
    UNWIND_FRAME_SIGRETURN = -1,    /* special sigreturn frame */
    UNWIND_FRAME_OTHER = 0,         /* not cacheable (special or unrecognised) */
    UNWIND_FRAME_GUESSED = 1        /* guessed it was regular, but not known */
};

/**
 * @brief Unwind mode
 */
enum UnwindMode {
    /** Dwarf unwind */
    DWARF_UNWIND = 0,
    /** Frame pointer unwind */
    FRAMEPOINTER_UNWIND,
    /** Quick unwind table */
    QUICKEN_UNWIND,
};

/**
 * @brief Unwind cache mode
 */
enum UnwindCachingPolicy : uint8_t {
    /** unwind no cache */
    UNWIND_CACHE_NONE = 0,
    /** unwind global cache */
    UNWIND_CACHE_GLOBAL,
    /** unwind per-thread cache */
    UNWIND_CACHE_PER_THREAD,
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
