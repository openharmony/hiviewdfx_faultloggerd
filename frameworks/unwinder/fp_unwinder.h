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

#include <vector>
#include <link.h>
#include <unistd.h>
#include <libunwind.h>
#include "backtrace.h"

namespace OHOS {
namespace HiviewDFX {
class FpUnwinder {
public:
    FpUnwinder();
    ~FpUnwinder();

    bool UnwindWithContext(unw_context_t& context, size_t skipFrameNum);
    bool Unwind(size_t skipFrameNum);
    void UpdateFrameInfo();
    const std::vector<NativeFrame>& GetFrames() const;
private:
    bool Step(uintptr_t& fp, uintptr_t& pc);
    static int DlIteratePhdrCallback(struct dl_phdr_info *info, size_t size, void *data);

private:
    std::vector<NativeFrame> frames_;
    uintptr_t stackBottom_;
    uintptr_t stackTop_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
