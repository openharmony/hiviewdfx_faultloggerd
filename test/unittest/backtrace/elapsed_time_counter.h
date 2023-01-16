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

#ifndef ELAPSED_TIME_COUNTER_H
#define ELAPSED_TIME_COUNTER_H

#include <chrono>

namespace OHOS {
namespace HiviewDFX {
class ElapsedTimeCounter {
public:
    ElapsedTimeCounter()
    {
        begin_ = std::chrono::steady_clock::now();
    };

    ~ElapsedTimeCounter() {};

    void Reset()
    {
        begin_ = std::chrono::steady_clock::now();
    };

    void Stop()
    {
        end_ = std::chrono::steady_clock::now();
    };

    int64_t CountInNanoseconds()
    {
        std::chrono::duration<int64_t, std::nano> elapsed = end_ - begin_;
        return elapsed.count();
    };

private:
    std::chrono::time_point<std::chrono::steady_clock> begin_;
    std::chrono::time_point<std::chrono::steady_clock> end_;
};
}
}
#endif // ELAPSED_TIME_COUNTER_H
