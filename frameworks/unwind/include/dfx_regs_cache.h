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
#ifndef DFX_REGS_CACHE_H
#define DFX_REGS_CACHE_H

#include <cstdint>
#include <string>
#include <memory>
#include "dfx_define.h"
#include "dfx_regs.h"

namespace OHOS {
namespace HiviewDFX {
class DfxRegsCache {
public:
    DfxRegsCache(DfxRegs* regs) : regs_(regs) {}
    DfxRegsCache(std::shared_ptr<DfxRegs> regs) : regs_(regs) {}
    virtual ~DfxRegsCache() = default;

    inline uintptr_t Get(uint32_t reg) {
        if (IsSaved(reg)) {
            return savedRegs[reg];
        }
        return (*(regs_->GetReg(reg)));
    }

    inline uintptr_t* Save(uint32_t reg) {
        if (reg >= REGS_MAX_SIZE) {
            return nullptr;
        }
        savedRegMask |= 1ULL << reg;
        savedRegs[reg] = *(regs_->GetReg(reg));
        return regs_->GetReg(reg);
    }

    inline bool IsSaved(uint32_t reg) {
        if (reg > REGS_MAX_SIZE) {
            return false;
        }
        return savedRegMask & (1ULL << reg);
    }

    inline uint16_t TotalSize() {
        return regs_->RegsSize();
    }
protected:
    std::shared_ptr<DfxRegs> regs_ = nullptr;
    uint64_t savedRegMask = 0;
    uintptr_t savedRegs[REGS_MAX_SIZE];
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
