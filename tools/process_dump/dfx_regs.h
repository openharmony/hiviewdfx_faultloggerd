/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include <memory>
#include <sys/types.h>
#include <ucontext.h>
#include <vector>
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
#define FP_MINI_REGS_SIZE 4

class DfxRegs {
public:
    DfxRegs() = default;
    virtual ~DfxRegs() {};
    static std::shared_ptr<DfxRegs> Create();
    static std::shared_ptr<DfxRegs> CreateFromContext(const ucontext_t &context);

    std::vector<uintptr_t> GetRegsData() const;
    void SetRegsData(const std::vector<uintptr_t>& regs);

    virtual std::string PrintRegs() const = 0;
    virtual std::string GetSpecialRegisterName(uintptr_t val) const = 0;
    virtual void GetFramePointerMiniRegs(void *regs) = 0;
    virtual uintptr_t GetPC() const = 0;
    virtual uintptr_t GetLR() const = 0;

    int PrintFormat(char *buf, int size, const char *format, ...) const;
protected:
    std::vector<uintptr_t> regsData_ {};
};

class DfxRegsArm : public DfxRegs {
public:
    explicit DfxRegsArm(const ucontext_t &context);
    DfxRegsArm() = default;
    ~DfxRegsArm() override {};
    std::string PrintRegs() const override;
    std::string GetSpecialRegisterName(uintptr_t val) const override;
    void GetFramePointerMiniRegs(void *regs) override;
    uintptr_t GetPC() const override;
    uintptr_t GetLR() const override;
};

class DfxRegsArm64 : public DfxRegs {
public:
    explicit DfxRegsArm64(const ucontext_t &context);
    DfxRegsArm64() = default;
    ~DfxRegsArm64() override {};
    std::string PrintRegs() const override;
    std::string GetSpecialRegisterName(uintptr_t val) const override;
    void GetFramePointerMiniRegs(void *regs) override;
    uintptr_t GetPC() const override;
    uintptr_t GetLR() const override;
};

class DfxRegsX86_64 : public DfxRegs {
public:
    explicit DfxRegsX86_64(const ucontext_t &context);
    DfxRegsX86_64() = default;
    ~DfxRegsX86_64() override {};
    std::string PrintRegs() const override;
    std::string GetSpecialRegisterName(uintptr_t val) const override;
    void GetFramePointerMiniRegs(void *regs) override;
    uintptr_t GetPC() const override;
    uintptr_t GetLR() const override;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
