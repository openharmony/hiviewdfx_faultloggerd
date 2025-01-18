/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef FP_UNWINDER_H
#define FP_UNWINDER_H
#include <cinttypes>
#include "dfx_define.h"
namespace OHOS {
namespace HiviewDFX {
class FpUnwinder {
public:
    FpUnwinder();
    ~FpUnwinder();
    static inline AT_ALWAYS_INLINE void GetFpPcRegs(void *regs)
    {
#if defined(__aarch64__)
        asm volatile(
        "1:\n"
        "adr x12, 1b\n"
        "stp x12, x29, [%[base], #0]\n"
        : [base] "+r"(regs)
        :
        : "x12", "memory");
#endif
    }
    static int32_t Unwind(uintptr_t* pcs, int32_t sz, int32_t skipFrameNum);
private:
    static int32_t UnwindFallback(uintptr_t* pcs, int32_t sz, int32_t skipFrameNum);
    static bool ReadUintptrSafe(int pipeWrite, uintptr_t addr, uintptr_t& value);
};
}
} // namespace OHOS
#endif