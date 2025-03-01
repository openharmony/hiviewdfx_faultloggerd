/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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
#ifndef STACK_UTILS_H
#define STACK_UTILS_H

#include <cstdio>
#include <csignal>
#include <cstring>
#include <memory>

namespace OHOS {
namespace HiviewDFX {

class StackUtils {
public:
    static StackUtils& Instance();

    StackUtils(const StackUtils&) = delete;
    StackUtils& operator=(const StackUtils&) = delete;

    bool GetMainStackRange(uintptr_t& stackBottom, uintptr_t& stackTop) const;
    bool GetArkStackRange(uintptr_t& start, uintptr_t& end) const;
    static bool GetSelfStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);
    static bool GetSigAltStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);
private:
    struct MapRange {
        bool IsValid() const
        {
            return start != 0 && start < end;
        }
        uintptr_t start{0};
        uintptr_t end{0};
    };
    StackUtils();
    void ParseSelfMaps();
    MapRange mainStack_;
    MapRange arkCode_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
