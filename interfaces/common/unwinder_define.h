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

namespace OHOS {
namespace HiviewDFX {
enum ArchType : uint8_t {
    ARCH_UNKNOWN = 0,
    ARCH_ARM,
    ARCH_ARM64,
    ARCH_X86,
    ARCH_X86_64,
};

enum UnwinderMode {
    DWARF_UNWIND = 0,                   // Dwarf
    FRAMEPOINTER_UNWIND,                // FramePointer
    QUICKEN_UNWIND,                     // Quicken
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
