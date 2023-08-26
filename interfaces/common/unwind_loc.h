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
#ifndef UNWIND_LOC_H
#define UNWIND_LOC_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
struct DwarfLoc {
    uintptr_t val = 0;
    uintptr_t type = 0;            /* see DWARF_LOC_TYPE_* macros.  */
};

#define DWARF_GET_LOC(l)       ((l).val)
#define DWARF_LOC_TYPE_MEM     (0 << 0)
#define DWARF_LOC_TYPE_FP      (1 << 0)
#define DWARF_LOC_TYPE_REG     (1 << 1)
#define DWARF_LOC_TYPE_VAL     (1 << 2)

} // namespace HiviewDFX
} // namespace OHOS
#endif
