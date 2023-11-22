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

#ifndef DFX_PID_UTILS_H
#define DFX_PID_UTILS_H

#include <cstdint>
#include <functional>
#include <sys/types.h>

namespace OHOS {
namespace HiviewDFX {

enum PidRunEnum : uint8_t {
    PID_RUN_KEEP_GOING,
    PID_RUN_PASS,
    PID_RUN_FAIL,
};

class PidUtils {
public:

    static bool Quiesce(pid_t pid);

    static bool Attach(pid_t pid);

    static bool Detach(pid_t pid);

    static bool WaitForPidState(pid_t pid, const std::function<PidRunEnum()>& stateCheckFunc);

    static bool WaitForPidStateAfterAttach(pid_t pid, const std::function<PidRunEnum()>& stateCheckFunc);
};
}
}
#endif
