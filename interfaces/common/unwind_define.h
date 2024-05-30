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
#ifndef UNWIND_DEFINE_H
#define UNWIND_DEFINE_H

#include <cinttypes>
#include <string>
#include <unistd.h>
#if defined(__arm__)
#include "unwind_arm_define.h"
#elif defined(__aarch64__)
#include "unwind_arm64_define.h"
#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
#include "unwind_riscv64_define.h"
#elif defined(__x86_64__)
#include "unwind_x86_64_define.h"
#elif defined(__loongarch_lp64)
#include "unwind_loongarch64_define.h"
#else
#error "Unsupported architecture"
#endif

namespace OHOS {
namespace HiviewDFX {
#define FP_MINI_REGS_SIZE 4
#define QUT_MINI_REGS_SIZE 7

#define ARM_EXIDX_TABLE_SIZE 8

static const int FRAME_MAX_SIZE = 64;

/**
 * @brief chip architecture
 */
enum ArchType : uint8_t {
    ARCH_UNKNOWN = 0,
    ARCH_ARM,
    ARCH_ARM64,
    ARCH_RISCV64,
    ARCH_X86,
    ARCH_X86_64,
    ARCH_LOONGARCH,
};

enum UnwindType : int8_t {
    UNWIND_TYPE_CUSTOMIZE_LOCAL = -3,
    UNWIND_TYPE_CUSTOMIZE = -2,
    UNWIND_TYPE_LOCAL = -1,
    UNWIND_TYPE_REMOTE,
};

enum UnwindDynInfoFormatType {
    UNW_INFO_FORMAT_TABLE,              /* unw_dyn_table_t */
    UNW_INFO_FORMAT_REMOTE_TABLE,       /* unw_dyn_remote_table_t */
    UNW_INFO_FORMAT_ARM_EXIDX,          /* ARM specific unwind info */
};

/**
 * @brief Unwind regs type
 */
enum UnwindRegsType {
    /** Dwarf */
    REGS_TYPE_DWARF = 0,
    /** Qut */
    REGS_TYPE_QUT,
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
    MINIMAL_UNWIND,
    /** Mix unwind table */
    MIX_UNWIND,
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
