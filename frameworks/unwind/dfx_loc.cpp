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

#include "dfx_loc.h"
#include <algorithm>
#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxLoc"
}

void DfxLocLocal::Clear(struct DwarfLoc& loc)
{
    loc.val = 0;
}

bool DfxLocLocal::IsNull(struct DwarfLoc loc)
{
    return (loc.val == 0);
}

void DfxLocLocal::Set(struct DwarfLoc& loc, uintptr_t val, uintptr_t type)
{
    loc.val = val;
}

void DfxLocLocal::SetReg(struct DwarfLoc& loc, int reg, void *arg)
{
    auto addr = DfxLocLocal::RegToAddr(reg, arg);
    if (addr == nullptr) {
        return;
    }
    loc.val = (uintptr_t) addr;
}

void DfxLocLocal::SetMem(struct DwarfLoc& loc, uintptr_t addr, void *arg)
{
    loc.val = addr;
}

bool DfxLocLocal::IsMem(struct DwarfLoc loc)
{
    return true;
}

bool DfxLocLocal::IsReg(struct DwarfLoc loc)
{
    return false;
}

bool DfxLocLocal::IsFpReg(struct DwarfLoc loc)
{
    return false;
}

void DfxLocRemote::Clear(struct DwarfLoc& loc)
{
    loc.val = 0;
    loc.type = 0;
}

bool DfxLocRemote::IsNull(struct DwarfLoc loc)
{
    return loc.val == 0 && loc.type == 0;
}

void DfxLocRemote::Set(struct DwarfLoc& loc, uintptr_t val, uintptr_t type)
{
    loc.val = val;
    loc.type = type;
}

void DfxLocRemote::SetReg(struct DwarfLoc& loc, int reg, void *arg)
{
    loc.val = reg;
    loc.type = DWARF_LOC_TYPE_REG;
}

void DfxLocRemote::SetMem(struct DwarfLoc& loc, uintptr_t addr, void *arg)
{
    loc.val = addr;
    loc.type = DWARF_LOC_TYPE_MEM;
}

bool DfxLocRemote::IsMem(struct DwarfLoc loc)
{
    if (loc.type == DWARF_LOC_TYPE_MEM) {
        return true;
    }
    return false;
}

bool DfxLocRemote::IsReg(struct DwarfLoc loc)
{
    if ((loc.type & DWARF_LOC_TYPE_REG) != 0) {
        return true;
    }
    return false;
}

bool DfxLocRemote::IsFpReg(struct DwarfLoc loc)
{
    if ((loc.type & DWARF_LOC_TYPE_FP) != 0) {
        return true;
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
