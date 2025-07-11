/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LPERF_RECORD_H
#define LPERF_RECORD_H

#include <string>
#include <vector>

#include "lperf_events.h"
#include "stack_printer.h"

namespace OHOS {
namespace HiviewDFX {
class LperfRecord {
public:
    int StartProcessSampling(int pid, const std::vector<int>& tids, int freq, int duration);
    int CollectSampleStack(std::string& datas);
    void FinishProcessSampling();

private:
    int OnSubCommand();
    void PrepareLperfEvent();
    void SymbolicRecord(LperfRecordSample& record);

    LperfEvents lperfEvents_;
    std::unique_ptr<StackPrinter> stackPrinter_ = nullptr;

    unsigned int timeStopSec_ = 0;
    unsigned int frequency_ = 0;
    int pid_ = 0;
    std::vector<int> tids_ = {};
};

template<typename T>
inline bool CheckOutOfRange(const T& value, const T& min, const T& max)
{
    return value < min || value > max;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif // LPERF_RECORD_H