/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base_unwinder.h"
#include "dfx_demangle.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxBaseUnwinder"
}

void BaseUnwinder::Init()
{
    lastErrorData_.code = UNW_ERROR_NONE;
    lastErrorData_.addr = 0;
    frames_.clear();
}

void BaseUnwinder::Destroy()
{
    frames_.clear();
}

bool BaseUnwinder::IsValidFrame(uintptr_t frame, uintptr_t stackTop, uintptr_t stackBottom)
{
    if (UNLIKELY(stackTop < stackBottom)) {
        return false;
    }
    return ((frame >= stackBottom) && (frame < stackTop - sizeof(uintptr_t)));
}
} // namespace HiviewDFX
} // namespace OHOS