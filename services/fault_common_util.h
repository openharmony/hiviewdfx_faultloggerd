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

#ifndef FAULT_COMMON_UTIL_H
#define FAULT_COMMON_UTIL_H

#include <sys/socket.h>
#include "csignal"
#include "cstdint"

namespace OHOS {
namespace HiviewDFX {
namespace FaultCommonUtil {
int32_t SendSignalToHapWatchdogThread(pid_t pid, const siginfo_t& si);
int32_t SendSignalToProcess(pid_t pid, const siginfo_t& si);

bool GetUcredByPeerCred(struct ucred& rcred, int32_t connectionFd);
}
}
}

#endif