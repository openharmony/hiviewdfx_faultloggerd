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
#ifndef UNWINDER_H
#define UNWINDER_H

#include <memory>
#include "base_unwinder.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class Unwinder {
public:
    Unwinder() = default;
    ~Unwinder();

    UnwindMode GetUnwinderMode() { return mode_; }
    void SetUnwinderMode(UnwindMode mode) { mode_ = mode; }

    void SetTargetPid(int pid);
    uintptr_t GetPcAdjustment();

    int UnwindInitLocal(UnwindContext* context);
    int UnwindInitRemote(UnwindContext* context);

    int Unwind(std::vector<DfxFrame> &frames);

    int SearchUnwindTable(uintptr_t pc, UnwindDynInfo *di, UnwindProcInfo *pi, int needUnwindInfo, void *arg);
    bool GetSymbolInfo(uint64_t pc, std::string* funcName, uint64_t* symStart, uint64_t* symEnd);
private:
    bool Init();
    void Destroy();

    UnwindMode mode_ = DWARF_UNWIND;
    std::shared_ptr<BaseUnwinder> unwinder_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
