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
#ifndef UNWINDER_CONTEXT_H
#define UNWINDER_CONTEXT_H

#include <memory>
#include <ucontext.h>
#include "byte_order.h"
#include "dfx_errors.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
struct UnwindConfig {
    size_t frameMaxSize {FRAME_MAX_SIZE};
    size_t frameSkipSize {0};
    int bigEndian = UNWIND_BYTE_ORDER;
    UnwindCachingPolicy cachingPolicy {UNWIND_CACHE_NONE};
};

struct UnwindCursor
{
    uintptr_t opaque[UNWIND_CURSOR_LEN];
};

struct UnwindDynTableInfo
{
    uintptr_t namePtr;
    uintptr_t segbase;
    uintptr_t tableLen;
    uintptr_t* tableData;
};

struct UnwindDynRemoteTableInfo
{
    uintptr_t namePtr;
    uintptr_t segbase;
    uintptr_t tableLen;
    uintptr_t tableData;
};

struct UnwindDynInfo {
    UnwindDynInfo *next;
    UnwindDynInfo *prev;
    uintptr_t startPc;
    uintptr_t endPc;
    uintptr_t gp;
    int format;
    union
    {
        UnwindDynTableInfo ti;
        UnwindDynRemoteTableInfo rti;
    } u;

};

struct ElfDynInfo
{
    uintptr_t startPc;
    uintptr_t endPc;
    UnwindDynInfo diCache;
    UnwindDynInfo diDebug;    /* additional table info for .debug_frame */
#if defined(__arm__)
    UnwindDynInfo diArm;      /* additional table info for .ARM.exidx */
#endif
};

struct UnwindProcInfo {
    uintptr_t startPc;
    uintptr_t endPc;
    uintptr_t lsda;
    uintptr_t handler;
    uintptr_t gp;
    int format;
    int unwindInfoSize;
    void *unwindInfo;
};

struct UnwindAccessors {
    int (*FindProcInfo)(uintptr_t, UnwindProcInfo *, int, void *);
    int (*AccessMem)(uintptr_t, uintptr_t *, int, void *);
    int (*AccessReg)(int, uintptr_t *, int, void *);
};

struct UnwindAddrSpace {
    UnwindAccessors acc;
    UnwindConfig* config;
    UnwindCursor* cursor;
    int pid;
};

struct UnwindContext {
    UnwindAddrSpace* as;
    void* context;
};

struct UnwindLocalContext {
    int regsSize;
    uintptr_t *regs;
};

struct UnwindPtraceContext
{
    pid_t pid;
    struct ElfDynInfo edi;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
