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
#include <ucontext.h>
#include "byte_order.h"
#include "dfx_errors.h"
#include "unwind_define.h"
#include "unwind_loc.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMap;
class DfxRegs;

struct UnwindTableInfo {
    UnwindTableInfo *next;
    UnwindTableInfo *prev;
    uintptr_t startPc;
    uintptr_t endPc;
    uintptr_t gp; /* global-pointer in effect for this entry */
    int format = -1;
    uintptr_t hint;
    uintptr_t namePtr;
    uintptr_t segbase;
    uintptr_t tableLen;
    uintptr_t tableData;
};

struct ElfTableInfo {
    uintptr_t startPc;
    uintptr_t endPc;
    UnwindTableInfo diEhHdr;
    UnwindTableInfo diDebug;    /* additional table info for .debug_frame */
#if defined(__arm__)
    UnwindTableInfo diExidx;      /* additional table info for .ARM.exidx */
#endif
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
};

struct UnwindContext {
    uintptr_t stackBottom;
    uintptr_t stackTop;
    int pid;
    std::shared_ptr<DfxRegs> regs = nullptr;
    std::shared_ptr<DfxMap> map = nullptr;
    struct UnwindTableInfo di;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
