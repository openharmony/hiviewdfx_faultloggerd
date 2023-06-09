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
#include "unwinder_define.h"

namespace OHOS {
namespace HiviewDFX {
#define FP_MINI_REGS_SIZE 4
#define QUT_MINI_REGS_SIZE 7

class DfxRegs {
public:
    DfxRegs() = default;
    virtual ~DfxRegs() {};
    static std::shared_ptr<DfxRegs> Create(int mode = UnwinderMode::DWARF_UNWIND);
    static std::shared_ptr<DfxRegs> CreateFromContext(const ucontext_t &context);

    inline uintptr_t& operator[](size_t reg) { return regsData_[reg]; }

    void* RawData() { return regsData_.data(); }
    std::vector<uintptr_t> GetRegsData() const;
    void SetRegsData(const std::vector<uintptr_t>& regs);

    virtual std::string PrintRegs() const = 0;
    virtual void GetFramePointerMiniRegs(void *regs) = 0;
    virtual void GetQuickenMiniRegs(void *regs) = 0;

    std::string GetSpecialRegisterName(uintptr_t val) const;
protected:
    std::string PrintSpecialRegs() const;
public:
    uintptr_t fp_ {0};
    uintptr_t pc_ {0};
    uintptr_t sp_ {0};
    uintptr_t lr_ {0};
protected:
    std::vector<uintptr_t> regsData_ {};
};

class DfxRegsArm : public DfxRegs {
public:
    explicit DfxRegsArm(const ucontext_t &context);
    DfxRegsArm() = default;
    ~DfxRegsArm() override {};
    std::string PrintRegs() const override;
    void GetFramePointerMiniRegs(void *regs) override;
    void GetQuickenMiniRegs(void *regs) override;
};

class DfxRegsArm64 : public DfxRegs {
public:
    explicit DfxRegsArm64(const ucontext_t &context);
    DfxRegsArm64() = default;
    ~DfxRegsArm64() override {};
    std::string PrintRegs() const override;
    void GetFramePointerMiniRegs(void *regs) override;
    void GetQuickenMiniRegs(void *regs) override;
};

class DfxRegsX86_64 : public DfxRegs {
public:
    explicit DfxRegsX86_64(const ucontext_t &context);
    DfxRegsX86_64() = default;
    ~DfxRegsX86_64() override {};
    std::string PrintRegs() const override;
    void GetFramePointerMiniRegs(void *regs) override;
    void GetQuickenMiniRegs(void *regs) override;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
