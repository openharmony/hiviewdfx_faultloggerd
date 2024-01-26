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
#ifndef FP_UNWINDER_H
#define FP_UNWINDER_H

#include <memory>
#include <vector>
#include <link.h>
#include <unistd.h>
#include <libunwind.h>
#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_regs.h"

namespace OHOS {
namespace HiviewDFX {
class FpUnwinder {
public:
    FpUnwinder();
    FpUnwinder(uintptr_t pcs[], int32_t sz);
    ~FpUnwinder();

    bool UnwindWithContext(unw_context_t& context, size_t skipFrameNum, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);
    bool Unwind(size_t skipFrameNum, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);
    bool Unwind(const std::shared_ptr<DfxRegs> &dfxregs, size_t skipFrameNum,
        size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

    void UpdateFrameInfo();
    const std::vector<DfxFrame>& GetFrames() const;
private:
    bool Step(uintptr_t& fp, uintptr_t& pc);
    inline bool IsValidFrame(uintptr_t frame, uintptr_t stackTop, uintptr_t stackBottom)
    {
        return ((frame > stackBottom) && (frame < stackTop - sizeof(uintptr_t)));
    }
    static int DlIteratePhdrCallback(struct dl_phdr_info *info, size_t size, void *data);

private:
    std::vector<DfxFrame> frames_;
    uintptr_t stackBottom_;
    uintptr_t stackTop_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
