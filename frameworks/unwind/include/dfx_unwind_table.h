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
#ifndef DFX_UNWIND_TABLE_H
#define DFX_UNWIND_TABLE_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "dfx_elf.h"
#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
class DfxUnwindTable final {
public:
    static int FindUnwindTable(struct ElfDynInfo* edi, uintptr_t pc, std::shared_ptr<DfxElf> elf);
    static int FindUnwindTable2(struct ElfDynInfo* edi, uintptr_t pc);

    static int SearchUnwindTable(struct UnwindProcInfo* pi, struct UnwindDynInfo* di,\
        uintptr_t pc, DfxMemory* memory, bool needUnwindInfo = false);

private:
    static int ResetElfDynInfo(struct ElfDynInfo* edi);
    static bool IsPcInUnwindInfo(struct UnwindDynInfo di, uintptr_t pc);

    static int ExdixSearchUnwindTable(struct UnwindProcInfo* pi, struct UnwindDynInfo* di,\
        uintptr_t pc, DfxMemory* memory, bool needUnwindInfo = false);
    static int DwarfSearchUnwindTable(struct UnwindProcInfo* pi, struct UnwindDynInfo* di,\
        uintptr_t pc, DfxMemory* memory, bool needUnwindInfo = false);

private:
    static std::unordered_map<size_t, uintptr_t> exidxAddrs_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
