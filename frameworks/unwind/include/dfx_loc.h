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
#ifndef DFX_LOC_H
#define DFX_LOC_H

#include <atomic>
#include <cstdint>
#include <string>
#include "dfx_define.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DfxLoc
{
public:
    DfxLoc() = default;
    virtual ~DfxLoc() = default;

    virtual void Clear(struct DwarfLoc& loc) = 0;
    virtual bool IsNull(struct DwarfLoc loc) = 0;
    virtual bool IsMem(struct DwarfLoc loc) = 0;
    virtual bool IsReg(struct DwarfLoc loc) = 0;
    virtual bool IsFpReg(struct DwarfLoc loc) = 0;
    virtual void Set(struct DwarfLoc& loc, uintptr_t val, uintptr_t type) = 0;
    virtual void SetReg(struct DwarfLoc& loc, int reg, void *arg) = 0;
    virtual void SetMem(struct DwarfLoc& loc, uintptr_t addr, void *arg) = 0;
};

class DfxLocLocal : public DfxLoc
{
public:
    DfxLocLocal() = default;
    virtual ~DfxLocLocal() = default;

    static inline void* RegToAddr(int reg, void *arg)
    {
        UnwindLocalContext* ctx = reinterpret_cast<UnwindLocalContext *>(arg);
        if (ctx == nullptr || ctx->regs == nullptr) {
            return nullptr;
        }
        if (reg < 0 || reg >= ctx->regsSize) {
            return nullptr;
        }
        return &(ctx->regs[reg]);
    }

    void Clear(struct DwarfLoc& loc) override;
    bool IsNull(struct DwarfLoc loc) override;
    bool IsMem(struct DwarfLoc loc) override;
    bool IsReg(struct DwarfLoc loc) override;
    bool IsFpReg(struct DwarfLoc loc) override;
    void Set(struct DwarfLoc& loc, uintptr_t val, uintptr_t type) override;
    void SetReg(struct DwarfLoc& loc, int reg, void *arg) override;
    void SetMem(struct DwarfLoc& loc, uintptr_t addr, void *arg) override;
};

class DfxLocRemote : public DfxLoc
{
public:
    DfxLocRemote() = default;
    virtual ~DfxLocRemote() = default;

    void Clear(struct DwarfLoc& loc) override;
    bool IsNull(struct DwarfLoc loc) override;
    bool IsMem(struct DwarfLoc loc) override;
    bool IsReg(struct DwarfLoc loc) override;
    bool IsFpReg(struct DwarfLoc loc) override;
    void Set(struct DwarfLoc& loc, uintptr_t val, uintptr_t type) override;
    void SetReg(struct DwarfLoc& loc, int reg, void *arg) override;
    void SetMem(struct DwarfLoc& loc, uintptr_t addr, void *arg) override;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
