/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef DFX_BACKTRACE_H
#define DFX_BACKTRACE_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
constexpr int32_t BACKTRACE_CURRENT_THREAD = -1;
struct NativeFrame {
    size_t index {0};
    uint64_t pc {0};
    uint64_t relativePc {0};
    uint64_t sp {0};
    uint64_t fp {0};
    uint64_t funcOffset {0};
    std::string binaryName {""};
    std::string buildId {""};
    std::string funcName {""};
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // DFX_BACKTRACE_H
