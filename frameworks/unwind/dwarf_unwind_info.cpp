

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

#include "dwarf_unwind_info.h"
#include "dfx_log.h"
#include "dwarf_define.h"
#include "dwarf_parser.h"
#include "dwarf_cfa_instructions.h"
#include "dfx_instructions.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfUnwindInfo"
}

bool DwarfUnwindInfo::Eval(uintptr_t fdeAddr, uintptr_t segbase,
    std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    LOGU("Eval: fdeAddr:%p, segbase:%p", (void*)fdeAddr , (void*)segbase);
    lastErrorData_.code = UNW_ERROR_NONE;
    lastErrorData_.addr = fdeAddr;
    // 3.parse cie and fde
    CommonInfoEntry cieInfo;
    FrameDescEntry fdeInfo;
    DwarfParser parser(memory_);
    if (!parser.ParseFDE(fdeAddr, fdeInfo, cieInfo, segbase)) {
        LOGE("Failed to parse fde?");
        lastErrorData_.code = UNW_ERROR_DWARF_INVALID_FDE;
        return false;
    }

    LOGU("pc: %p, FDE start: %p", (void*) regs->GetPc(), (void*) (fdeInfo.pcStart) );
    // 4. parse dwarf instructions and get cache rs
    DwarfCfaInstructions dwarfInstructions(memory_);
    if (!dwarfInstructions.Parse(regs->GetPc(), cieInfo, fdeInfo, *(rs.get()))) {
        LOGE("Failed to parse dwarf instructions?");
        lastErrorData_.code = UNW_ERROR_DWARF_INVALID_INSTR;
        return false;
    }

    // 5. update regs and regs state
    DfxInstructions instructions(memory_);
    bool ret = instructions.Apply(*(regs.get()), *(rs.get()));

    if (!IsPcSet()) {
        regs->SetReg(REG_PC, regs->GetReg(REG_LR));
    }
    return ret;
}
}   // namespace HiviewDFX
}   // namespace OHOS