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
#ifndef COREDUMP_CONTROLLER_H
#define COREDUMP_CONTROLLER_H
#include <string>
#include "dfx_dump_request.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpController {
public:
    static bool IsHwasanCoredumpEnabled();
    static bool IsCoredumpSignal(const ProcessDumpRequest& request);
    static bool IsCoredumpAllowed(const ProcessDumpRequest& request);
    static bool VerifyHap();
private:
    static bool VerifyTrustList(const std::string& bundleName);
    static std::string GetCoredumpHapList();
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
