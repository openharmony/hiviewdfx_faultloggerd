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

#include "arch_util.h"

#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
ArchType GetCurrentArch()
{
    ArchType curArch = ArchType::ARCH_UNKNOWN;
#if defined(__arm__)
    curArch = ArchType::ARCH_ARM;
#elif defined(__aarch64__)
    curArch = ArchType::ARCH_ARM64;
#elif defined(__riscv) && __riscv_xlen == 64
    curArch = ArchType::ARCH_RISCV64;
#elif defined(__i386__)
    curArch = ArchType::ARCH_X86;
#elif defined(__x86_64__)
    curArch = ArchType::ARCH_X86_64;
#else
#error "Unsupported architecture"
#endif
    return curArch;
}

ArchType GetArchFromUname(const std::string& machine)
{
    if (StartsWith(machine, "arm")) {
        if (machine == "armv8l") {
            return ArchType::ARCH_ARM64;
        }
        return ArchType::ARCH_ARM;
    } else if (machine == "aarch64") {
        return ArchType::ARCH_ARM64;
    } else if (machine == "riscv64") {
        return ArchType::ARCH_RISCV64;
    } else if (machine == "x86_64") {
        return ArchType::ARCH_X86_64;
    } else if (machine == "x86" || machine == "i686") {
        return ArchType::ARCH_X86;
    } else {
        return ArchType::ARCH_UNKNOWN;
    }
}

const std::string GetArchName(ArchType arch)
{
    switch (arch) {
        case ArchType::ARCH_X86:
            return "X86_32";
        case ArchType::ARCH_X86_64:
            return "X86_64";
        case ArchType::ARCH_ARM:
            return "ARM";
        case ArchType::ARCH_ARM64:
            return "ARM64";
        case ArchType::ARCH_RISCV64:
            return "RISCV64";
        default:
            return "Unsupport";
    }
}
}   // namespace HiviewDFX
}   // namespace OHOS
