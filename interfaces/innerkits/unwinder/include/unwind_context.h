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
#ifndef UNWIND_CONTEXT_H
#define UNWIND_CONTEXT_H

#include <memory>
#if is_ohos
#include <ucontext.h>
#endif

#include "byte_order.h"
#include "dfx_errors.h"
#include "dfx_regs_qut.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMap;
class DfxMaps;
class DfxRegs;

struct UnwindTableInfo {
    uintptr_t startPc = 0;
    uintptr_t endPc = 0;
    uintptr_t gp = 0; /* global-pointer in effect for this entry */
    int format = -1;
    bool isLinear = false;
    uintptr_t namePtr = 0;
    uintptr_t segbase = 0;
    uintptr_t tableLen = 0;
    uintptr_t tableData = 0;
};

struct UnwindEntryInfo {
    uintptr_t startPc;
    uintptr_t endPc;
    uintptr_t lsda;
    uintptr_t handler;
    uintptr_t gp;
    int format = -1;
    int unwindInfoSize;
    void *unwindInfo;
};

struct UnwindAccessors {
    int (*FindUnwindTable)(uintptr_t, UnwindTableInfo &, void *);
    int (*AccessMem)(uintptr_t, uintptr_t *, void *);
    int (*AccessReg)(int, uintptr_t *, void *);
    int (*GetMapByPc)(uintptr_t, std::shared_ptr<DfxMap> &, void *);
};

struct UnwindContext {
    bool stackCheck = false;
    uintptr_t stackBottom = 0;
    uintptr_t stackTop = 0;
    int pid = 0;
    std::shared_ptr<DfxRegs> regs = nullptr;
    std::shared_ptr<DfxMaps> maps = nullptr;
    std::shared_ptr<DfxMap> map = nullptr;
    struct UnwindTableInfo di;
};

enum RegLocEnum : uint8_t {
    REG_LOC_UNUSED = 0,     // register has same value as in prev. frame
    REG_LOC_UNDEFINED,      // register isn't saved at all
    REG_LOC_VAL_OFFSET,     // register that is the value, cfa = cfa + off
    REG_LOC_MEM_OFFSET,     // register saved at CFA-relative address, cfa = [r14 + off], r14 = [cfa + off]
    REG_LOC_REGISTER,       // register saved in another register, cfa = [r14], pc = [lr]
    REG_LOC_VAL_EXPRESSION, // register value is expression result, r11 = cfa + expr_result
    REG_LOC_MEM_EXPRESSION, // register stored in expression result, r11 = [cfa + expr_result]
};

struct RegLoc {
    RegLocEnum type = REG_LOC_UNUSED; /* see DWARF_LOC_* macros. */
    intptr_t val = 0;
};

// saved register status after running call frame instructions
// it should describe how register saved
struct RegLocState {
    RegLocState() { locs.resize(DfxRegsQut::GetQutRegsSize()); }
    explicit RegLocState(size_t locsSize) { locs.resize(locsSize); }
    ~RegLocState() = default;

    uintptr_t pcStart = 0;
    uintptr_t pcEnd = 0;
    uint32_t cfaReg = 0; // cfa = [r14]
    union {
        int32_t cfaRegOffset = 0; // cfa = cfa + offset
        uintptr_t cfaExprPtr; // cfa = expr
    };
    bool returnAddressUndefined = false;
    bool returnAddressSame = false;
    uint16_t returnAddressRegister = 0; // lr
    std::vector<RegLoc> locs;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
