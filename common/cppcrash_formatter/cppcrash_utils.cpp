/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "cppcrash_utils.h"

namespace OHOS {
namespace HiviewDFX {

bool CppCrashUtils::IsLastValidFrame(const DfxFrame& frame)
{
    static uintptr_t libcStartPc = 0;
    static uintptr_t libffrtStartEntry = 0;
    if (((libcStartPc != 0) && (frame.pc == libcStartPc)) ||
        ((libffrtStartEntry != 0) && (frame.pc == libffrtStartEntry))) {
        return true;
    }

    if (frame.mapName.find("ld-musl-aarch64.so.1") != std::string::npos &&
        frame.funcName.find("start") != std::string::npos) {
        libcStartPc = frame.pc;
        return true;
    }

    if (frame.mapName.find("libffrt") != std::string::npos &&
        frame.funcName.find("CoStartEntry") != std::string::npos) {
        libffrtStartEntry = frame.pc;
        return true;
    }
    return false;
}

} // namespace HiviewDFX
} // namespace OHOS