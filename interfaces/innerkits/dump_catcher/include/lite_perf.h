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
#ifndef LITE_PERF_H
#define LITE_PERF_H

#include <memory>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
class LitePerf {
public:
    LitePerf();
    ~LitePerf() = default;
    LitePerf(const LitePerf&) = delete;
    LitePerf& operator=(const LitePerf&) = delete;

    /**
     * @brief Start process stack sampling
     *
     * @param tids  thread ids
     * @param freq  freq
     * @param durationMs  duration ms
     * @param parseMiniDebugInfo whether parse MiniDebugInfo
     * @return if succeed return 0, otherwise return -1
    */
    int StartProcessStackSampling(const std::vector<int>& tids, int freq, int durationMs, bool parseMiniDebugInfo);

    /**
     * @brief Collect process stack sampling By tid
     *
     * @param tid  thread id
     * @param stack  stack of thread id
     * @return if succeed return 0, otherwise return -1
    */
    int CollectSampleStackByTid(int tid, std::string& stack);

    /**
     * @brief Finish process stack sampling
     *
     * @return if succeed return 0, otherwise return -1
    */
    int FinishProcessStackSampling();

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // LITE_PERF_H