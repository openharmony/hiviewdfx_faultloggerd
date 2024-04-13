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
#include <mutex>
#include "parameter.h"
#include "parameters.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#ifdef is_ohos_lite
[[maybe_unused]] constexpr bool g_defaultValue = false;
#else
[[maybe_unused]] constexpr bool g_defaultValue = true;
#endif
[[maybe_unused]] constexpr char BETA_ENABLED_KEY[] = "const.logsystemversion.type";
[[maybe_unused]] constexpr char ASYNCSTACK_ENABLED_KEY[] = "persist.faultloggerd.priv.asyncstack.enabled";
[[maybe_unused]] constexpr char MIXSTACK_ENABLED_KEY[] = "persist.faultloggerd.priv.mixstack.enabled";
}

#if defined(ENABLE_PARAMETER)
#define GEN_ENABLE_PARAM_FUNC(FuncName, EnableKey, DefValue) \
    __attribute__((noinline)) static bool FuncName() \
    { \
        static bool enable = DefValue; \
        static std::once_flag flag; \
        std::call_once(flag, [&] { \
            if (OHOS::system::GetParameter(EnableKey, "true") == "true") { \
                enable = true; \
            } else { \
                enable = false; \
            } \
        }); \
        return enable; \
    }
#else
#define GEN_ENABLE_PARAM_FUNC(FuncName, EnableKey, DefValue) \
    __attribute__((noinline)) static bool FuncName() \
    { \
        return DefValue; \
    }
#endif

class DfxParam {
public:
    GEN_ENABLE_PARAM_FUNC(EnableBeta, BETA_ENABLED_KEY, g_defaultValue);
    GEN_ENABLE_PARAM_FUNC(EnableAsyncStack, ASYNCSTACK_ENABLED_KEY, g_defaultValue);
    GEN_ENABLE_PARAM_FUNC(EnableMixstack, MIXSTACK_ENABLED_KEY, g_defaultValue);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif