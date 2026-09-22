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

#ifndef DUMP_CATCHER_CONSTANTS_H
#define DUMP_CATCHER_CONSTANTS_H

#include <cstdint>
#include <unistd.h>

namespace OHOS {
namespace HiviewDFX {
namespace DumpCatcherConstants {

constexpr int32_t HIVIEW_UID = 1201;
constexpr int32_t FOUNDATION_UID = 5523;

constexpr int32_t WORK_TIME_LIMIT_S = 60;
constexpr uint32_t WAIT_KERNEL_STACK_TIMEOUT_MS = 1000;
constexpr int32_t REMOTE_P90_TIMEOUT_MS = 1000;
constexpr int32_t REMOTE_TIMEOUT_MS = 10000;
constexpr int32_t MIN_REMOTE_TIMEOUT_MS = 1000;
constexpr int32_t SHORT_TIMEOUT_THRESHOLD_MS = 1000;

constexpr int32_t DUMP_CATCH_TID_ZERO_TIMEOUT_MS = 3000;
constexpr int32_t DUMP_CATCH_TID_NONZERO_TIMEOUT_MS = 10000;

constexpr size_t SKIP_FRAME_FOR_DUMP_LOCAL = 4;
constexpr size_t SKIP_FRAME_FOR_DUMP_PID = 5;
constexpr size_t SKIP_FRAME_FOR_DUMP_SELF = 1;

constexpr int32_t SIGNAL_MAX_NUM = 64;
constexpr int32_t SIGBLK_HEX_LEN = 16;
constexpr int32_t SIGBLK_FIELD_OFFSET = 2;

constexpr int32_t FD_STR_BUF_LEN = 16;
constexpr int32_t WAITPID_RETRY_COUNT = 150;
constexpr int32_t WAITPID_USLEEP_US = 100000;
constexpr int32_t SECOND_TO_MS = 1000;

} // namespace DumpCatcherConstants
} // namespace HiviewDFX
} // namespace OHOS

#endif // DUMP_CATCHER_CONSTANTS_H
