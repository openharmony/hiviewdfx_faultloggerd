/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "coredump_controller.h"

#include "coredump_config_manager.h"
#include "coredump_mapping_manager.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "parameters.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr const char* const COREDUMP_HAP_LIST = "const.dfx.coredump.hap_list";
    constexpr const char* const HWASAN_COREDUMP_ENABLE = "faultloggerd.priv.hwasan_coredump.enabled";
}

std::string CoredumpController::GetCoredumpHapList()
{
    static std::string whiteList = OHOS::system::GetParameter(COREDUMP_HAP_LIST, "");
    return whiteList;
}

bool CoredumpController::IsHwasanCoredumpEnabled()
{
    static bool isHwasanCoredumpEnabled = OHOS::system::GetParameter(HWASAN_COREDUMP_ENABLE, "false") == "true";
    return isHwasanCoredumpEnabled;
}

bool CoredumpController::IsCoredumpSignal(const ProcessDumpRequest& request)
{
    return request.siginfo.si_signo == SIGLEAK_STACK && abs(request.siginfo.si_code) == SIGLEAK_STACK_COREDUMP;
}

bool CoredumpController::VerifyTrustList(const std::string& bundleName)
{
    if (bundleName.empty()) {
        return false;
    }
    std::string hapList = GetCoredumpHapList();
    if (hapList.find(bundleName) != std::string::npos) {
        return true;
    }
    DFXLOGE("The bundleName %{public}s is not allowed to coredump", bundleName.c_str());
    return false;
}

bool CoredumpController::VerifyHap()
{
    if (VerifyTrustList(DumpUtils::GetSelfBundleName()) ||
        (IsHwasanCoredumpEnabled() && CoredumpMappingManager::GetInstance().IsHwAsanProcess())) {
        return true;
    }

    return CoredumpConfigManager::GetInstance().GetConfig().coredumpSwitch;
}

bool CoredumpController::IsCoredumpAllowed(const ProcessDumpRequest& request)
{
    if (IsCoredumpSignal(request) || (request.siginfo.si_signo == SIGABRT && IsHwasanCoredumpEnabled())) {
        return true;
    }
    return false;
}
}
}
