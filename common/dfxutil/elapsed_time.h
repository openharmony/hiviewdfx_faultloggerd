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
#include "dfx_log.h"
namespace OHOS {
namespace HiviewDFX {
class ElapsedTime {
public:
    ElapsedTime()
    {
        begin_ = std::chrono::high_resolution_clock::now();
    };

    ElapsedTime(std::string printContent, time_t limitCostMilliseconds)
        : limitCostMilliseconds_(limitCostMilliseconds), printContent_(std::move(printContent))
    {
        Reset();
    }

    ~ElapsedTime()
    {
        if (limitCostMilliseconds_ != 0) {
            time_t costTime = Elapsed<std::chrono::milliseconds>();
            if (costTime > limitCostMilliseconds_) {
                DFXLOG_WARN("%s running %lld ms", printContent_.c_str(), costTime);
            }
        }
    }

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
    time_t limitCostMilliseconds_ = 0;
    std::string printContent_ = "";
};
}
}
#endif
