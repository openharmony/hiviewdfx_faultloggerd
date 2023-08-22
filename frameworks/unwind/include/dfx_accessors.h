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
#ifndef DFX_ACCESSORS_H
#define DFX_ACCESSORS_H

#include <atomic>
#include <cstdint>
#include <string>
#include "dfx_define.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DfxAccessors
{
public:
    DfxAccessors(int bigEndian = UNWIND_BYTE_ORDER) : bigEndian_(bigEndian) {}
    virtual ~DfxAccessors() = default;

    virtual int AccessMem(uintptr_t addr, uintptr_t *val, int write, void *arg) = 0;
    virtual int AccessReg(int reg, uintptr_t *val, int write, void *arg) = 0;
    virtual int FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg) = 0;

    int bigEndian_ = UNWIND_BYTE_ORDER;
};

class DfxAccessorsLocal : public DfxAccessors
{
public:
    DfxAccessorsLocal() = default;
    virtual ~DfxAccessorsLocal() = default;

    int AccessMem(uintptr_t addr, uintptr_t *val, int write, void *arg) override;
    int AccessReg(int reg, uintptr_t *val, int write, void *arg) override;
    int FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg) override;
};

class DfxAccessorsRemote : public DfxAccessors
{
public:
    DfxAccessorsRemote() = default;
    virtual ~DfxAccessorsRemote() = default;

    int AccessMem(uintptr_t addr, uintptr_t *val, int write, void *arg) override;
    int AccessReg(int reg, uintptr_t *val, int write, void *arg) override;
    int FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg) override;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
