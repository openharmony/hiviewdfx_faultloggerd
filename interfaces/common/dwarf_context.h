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
#ifndef DWARF_CONTEXT_H
#define DWARF_CONTEXT_H

#include <memory>
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMap;

struct DwarfCursor {
    void *asArg;               /* argument to address-space callbacks */
    UnwindAddrSpace as;        /* reference to per-address-space info */

    uintptr_t cfa;     /* canonical frame address; aka frame-/stack-pointer */
    uintptr_t ip;              /* instruction pointer */

    unsigned int piValid :1;   /* is proc_info valid? */
    unsigned int piIsDynamic :1; /* proc_info found via dynamic proc info? */
    UnwindProcInfo pi;         /* info about current procedure */

    DwarfLoc loc[DWARF_PRESERVED_REGS_NUM];

    uintptr_t relPc; /* pc related to the beginning of the elf, used for addr2line */
    uintptr_t cachedIp;              /* cached instruction pointer */
    std::shared_ptr<DfxMap> map; /* memory mapping info associated with cached ip */
    int index;
    int regSz;
    uintptr_t regs[DWARF_PRESERVED_REGS_NUM];
    uintptr_t fp;     /* frame-pointer */
};

struct UnwindDwarfCursor {
    struct DwarfCursor dwarf;
    struct UnwindFrameInfo frameInfo;        /* quick tracing assist info */
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
