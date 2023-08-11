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
#ifndef BASE_UNWINDER_H
#define BASE_UNWINDER_H

#include <memory>
#include <vector>
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_frame.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class BaseUnwinder {
public:
    BaseUnwinder() = default;
    virtual ~BaseUnwinder() = default;

    virtual void UnwindInit(UnwindContext* context) = 0;
    virtual bool Unwind() = 0;

    virtual void Init();
    virtual void Destroy();
    UnwindContext* GetContext() { return context_; }
    const std::vector<DfxFrame>& GetFrames() { return frames_; }
    const uint16_t& GetLastErrorCode() const { return lastErrorData_.code; }
    const uint64_t& GetLastErrorAddr() const { return lastErrorData_.addr; }
protected:
    bool IsValidFrame(uintptr_t frame, uintptr_t stackTop, uintptr_t stackBottom);

protected:
    UnwindContext* context_;
    UnwindErrorData lastErrorData_;
    std::vector<DfxFrame> frames_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
