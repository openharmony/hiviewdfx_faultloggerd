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
#include "dfx_elf.h"
#include "dfx_memory.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
class DfxRegs {
public:
    explicit DfxRegs() : regsData_(REG_LAST) {}
    virtual ~DfxRegs() = default;

    static std::shared_ptr<DfxRegs> Create();
    static std::shared_ptr<DfxRegs> CreateFromUcontext(const ucontext_t& context);
    static std::shared_ptr<DfxRegs> CreateFromRegs(const UnwindMode mode, const uintptr_t* regs);
    static std::shared_ptr<DfxRegs> CreateRemoteRegs(pid_t pid);
    static void SetQutRegs(std::vector<uint16_t> qutRegs) { qutRegs_ = qutRegs; }
    static const std::vector<uint16_t>& GetQutRegs()
    {
        if (!qutRegs_.empty()) {
            return qutRegs_;
        }
        return QUT_REGS;
    };
    static bool IsQutReg(uint16_t reg)
    {
        const std::vector<uint16_t>& qutRegs = GetQutRegs();
        for (size_t i = 0; i < qutRegs.size(); ++i) {
            if (qutRegs[i] == reg) {
                return true;
            }
        }
        return false;
    };
    virtual void SetFromUcontext(const ucontext_t& context) = 0;
    virtual void SetFromFpMiniRegs(const uintptr_t* regs) = 0;
    virtual void SetFromQutMiniRegs(const uintptr_t* regs) = 0;
    virtual std::string PrintRegs() const = 0;
    virtual bool StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory) = 0;

    inline uintptr_t& operator[](size_t idx) { return regsData_[idx]; }

    void* RawData() { return regsData_.data(); }
    size_t RegsSize() const { return regsData_.size(); }
    std::vector<uintptr_t> GetRegsData() const;
    void SetRegsData(const std::vector<uintptr_t>& regsData);
    void SetRegsData(const uintptr_t* regs);
    uintptr_t* GetReg(size_t idx);
    void SetReg(const int idx, const uintptr_t* val);

    uintptr_t GetSp() const;
    void SetSp(uintptr_t sp);
    uintptr_t GetPc() const;
    void SetPc(uintptr_t pc);
#if is_ohos
    uintptr_t GetFp() const;
    void SetFp(uintptr_t fp);
    void GetSpecialRegs(uintptr_t& fp, uintptr_t& lr, uintptr_t& sp, uintptr_t& pc) const;
    void SetSpecialRegs(uintptr_t fp, uintptr_t lr, uintptr_t sp, uintptr_t pc);
#endif
    std::string GetSpecialRegsName(uintptr_t val) const;
    std::string PrintSpecialRegs() const;
protected:
    std::vector<uintptr_t> regsData_ {};
    static std::vector<uint16_t> qutRegs_;
};

#if defined(__arm__)
class DfxRegsArm : public DfxRegs {
public:
    DfxRegsArm() = default;
    ~DfxRegsArm() = default;
    void SetFromUcontext(const ucontext_t& context) override;
    void SetFromFpMiniRegs(const uintptr_t* regs) override;
    void SetFromQutMiniRegs(const uintptr_t* regs) override;
    std::string PrintRegs() const override;
    bool StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory) override;
};
#endif

#if defined(__aarch64__)
class DfxRegsArm64 : public DfxRegs {
public:
    DfxRegsArm64() = default;
    ~DfxRegsArm64() = default;
    void SetFromUcontext(const ucontext_t& context) override;
    void SetFromFpMiniRegs(const uintptr_t* regs) override;
    void SetFromQutMiniRegs(const uintptr_t* regs) override;
    std::string PrintRegs() const override;
    bool StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory) override;
};
#endif

#if defined(__x86_64__)
class DfxRegsX86_64 : public DfxRegs {
public:
    DfxRegsX86_64() = default;
    ~DfxRegsX86_64() = default;
    void SetFromUcontext(const ucontext_t& context) override;
    void SetFromFpMiniRegs(const uintptr_t* regs) override;
    void SetFromQutMiniRegs(const uintptr_t* regs) override;
    std::string PrintRegs() const override;
    bool StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory) override;
};
#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif
