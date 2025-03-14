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

#ifndef KERNEL_SNAPSHOT_UTIL_H
#define KERNEL_SNAPSHOT_UTIL_H
#include <string>

#include "kernel_snapshot_data.h"

namespace OHOS {
namespace HiviewDFX {
namespace KernelSnapshotUtil {
std::string FilterEmptySection(const std::string& secHead, const std::string& secCont, const std::string& end);
std::string FormatTimestamp(const std::string& timestamp);
std::string GetBuildInfo();
std::string FillSummary(CrashMap& output, bool isLocal = false);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
