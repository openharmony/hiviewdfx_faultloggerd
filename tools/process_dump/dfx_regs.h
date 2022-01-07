/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef DFX_REGS_H
#define DFX_REGS_H

#include <cstdint>
#include <vector>
#include <ucontext.h>

#include <sys/types.h>

#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
class DfxRegs {
public:
    DfxRegs() = default;
    virtual ~DfxRegs() {};
    std::vector<uintptr_t> GetRegsData() const
    {
        return regsData_;
    }
    virtual void PrintRegs(int32_t fd) const = 0;
    void SetRegs(const std::vector<uintptr_t> regs)
    {
        regsData_ = regs;
    }
private:
    std::vector<uintptr_t> regsData_ {};
};

class DfxRegsArm : public DfxRegs {
public:
    explicit DfxRegsArm(const ucontext_t &context);
    ~DfxRegsArm() override {};
    void PrintRegs(int32_t fd) const override;
private:
    DfxRegsArm() = delete;
};

class DfxRegsArm64 : public DfxRegs {
public:
    explicit DfxRegsArm64(const ucontext_t &context);
    ~DfxRegsArm64() override {};
    void PrintRegs(int32_t fd) const override;
private:
    DfxRegsArm64() = delete;
};

class DfxRegsX86_64 : public DfxRegs {
public:
    explicit DfxRegsX86_64(const ucontext_t &context);
    ~DfxRegsX86_64() override {};
    void PrintRegs(int32_t fd) const override;
private:
    DfxRegsX86_64() = delete;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
