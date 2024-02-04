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
#include "unwind_define.h"
#include "unwind_loc.h"

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
    int pid;
    std::shared_ptr<DfxRegs> regs = nullptr;
    std::shared_ptr<DfxMaps> maps = nullptr;
    std::shared_ptr<DfxMap> map = nullptr;
    struct UnwindTableInfo di;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
