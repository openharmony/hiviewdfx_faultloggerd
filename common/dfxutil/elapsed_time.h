/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#ifndef ELAPSED_TIME_H
#define ELAPSED_TIME_H

#include <chrono>

namespace OHOS {
namespace HiviewDFX {
class ElapsedTime {
public:
    ElapsedTime()
    {
        begin_ = std::chrono::high_resolution_clock::now();
    };

    ~ElapsedTime() {};

    void Reset()
    {
        begin_ = std::chrono::high_resolution_clock::now();
    };

    template<typename Duration=std::chrono::nanoseconds>
    time_t Elapsed() const
    {
        return std::chrono::duration_cast<Duration>(std::chrono::high_resolution_clock::now() - begin_).count();
    };

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_;
};
}
}
#endif // ELAPSED_TIME_COUNTER_H
