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
#include "dfx_map.h"
#include "dfx_memory.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
enum UnwindTableError : uint8_t {
    TABLE_ERROR_NONE = 0,
    TABLE_ERROR_FORMAT,
    TABLE_ERROR_PC_NOT_IN,
};

struct DlCbData {
    uintptr_t pc;
    ElfTableInfo edi;
};

class DfxUnwindTable final {
public:
    static bool GetElfTableInfo(struct ElfTableInfo& eti, uintptr_t pc,
        std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf);

    static int FindUnwindTable(struct ElfTableInfo& eti, uintptr_t pc,
        std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf);
    static int FindUnwindTableLocal(struct ElfTableInfo& eti, uintptr_t pc);

    static int SearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti,\
        uintptr_t pc, std::shared_ptr<DfxMemory> memory);

private:
    static int IsPcInUnwindTable(struct UnwindTableInfo uti, uintptr_t pc);
    static int IsPcInElfTable(struct ElfTableInfo eti, uintptr_t pc);
    static int ResetElfTableInfo(struct ElfTableInfo& edi);

    static int DlPhdrCb(struct dl_phdr_info *info, size_t size, void *data);
    static ElfW(Addr) FindSection(struct dl_phdr_info *info, const std::string secName);

    static bool GetExidxTableInfo(struct UnwindTableInfo& ti,
        std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf);
    static bool GetEhHdrTableInfo(struct UnwindTableInfo& ti,
        std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf);

    static int ExdixSearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti,\
        uintptr_t pc, std::shared_ptr<DfxMemory> memory);
    static int DwarfSearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti,\
        uintptr_t pc, std::shared_ptr<DfxMemory> memory);

private:
    static std::unordered_map<size_t, uintptr_t> exidxAddrs_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
