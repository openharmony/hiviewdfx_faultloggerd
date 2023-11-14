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
#ifndef DFX_REGS_QUT_H
#define DFX_REGS_QUT_H

#include <cstdint>
#include <vector>
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
class DfxRegsQut {
public:
    static inline void SetQutRegs(const std::vector<uint16_t>& qutRegs)
    {
        qutRegs_ = qutRegs;
    }

    static inline const std::vector<uint16_t>& GetQutRegs()
    {
        if (!qutRegs_.empty()) {
            return qutRegs_;
        }
        return QUT_REGS;
    }

    static inline size_t GetQutRegsSize()
    {
        return GetQutRegs().size();
    }

    static inline bool IsQutReg(uint16_t reg, size_t& qutIdx)
    {
        const std::vector<uint16_t>& qutRegs = GetQutRegs();
        for (size_t i = 0; i < qutRegs.size(); ++i) {
            if (qutRegs[i] == reg) {
                qutIdx = i;
                return true;
            }
        }
        return false;
    }

protected:
    static std::vector<uint16_t> qutRegs_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
