/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <string>
#include <sys/types.h>
#include <ucontext.h>
#include <vector>
#include "dfx_define.h"
#include "dfx_util.h"

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
    virtual std::string PrintRegs() const = 0;
    virtual std::string GetSpecialRegisterName(uintptr_t val) const = 0;
    virtual uintptr_t GetPC() const = 0;
    virtual uintptr_t GetLR() const = 0;
    void SetRegs(const std::vector<uintptr_t> regs)
    {
        regsData_ = regs;
    }
protected:
    std::vector<uintptr_t> regsData_ {};
};

class DfxRegsArm : public DfxRegs {
public:
    explicit DfxRegsArm(const ucontext_t &context);
    ~DfxRegsArm() override {};
    std::string PrintRegs() const override;
    std::string GetSpecialRegisterName(uintptr_t val) const override;
    uintptr_t GetPC() const override;
    uintptr_t GetLR() const override;
private:
    DfxRegsArm() = delete;
};

class DfxRegsArm64 : public DfxRegs {
public:
    explicit DfxRegsArm64(const ucontext_t &context);
    ~DfxRegsArm64() override {};
    std::string PrintRegs() const override;
    std::string GetSpecialRegisterName(uintptr_t val) const override;
    uintptr_t GetPC() const override;
    uintptr_t GetLR() const override;
private:
    DfxRegsArm64() = delete;
};

class DfxRegsX86_64 : public DfxRegs {
public:
    explicit DfxRegsX86_64(const ucontext_t &context);
    ~DfxRegsX86_64() override {};
    std::string PrintRegs() const override;
    std::string GetSpecialRegisterName(uintptr_t val) const override;
    uintptr_t GetPC() const override;
    uintptr_t GetLR() const override;
private:
    DfxRegsX86_64() = delete;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
