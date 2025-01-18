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
#ifndef DFX_PARAM_H
#define DFX_PARAM_H

#include <cstdint>
#if defined(ENABLE_PARAMETER)
#include "parameter.h"
#include "parameters.h"
#endif

namespace OHOS {
namespace HiviewDFX {

class DfxParam {
public:
    static bool IsEnableMixstack()
    {
#if defined(ENABLE_PARAMETER)
        static bool isEnable = [] {
            return OHOS::system::GetParameter("faultloggerd.priv.mixstack.enabled", "true") == "true";
        }();
        return isEnable;
#else
        return false;
#endif
    }
};
} // namespace HiviewDFX
} // namespace OHOS
#endif