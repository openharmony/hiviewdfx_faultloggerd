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

#ifndef RELIABILITY_STACK_PRINTER_H
#define RELIABILITY_STACK_PRINTER_H

#include <vector>

#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
struct TimeStampedPcs {
    uint64_t snapshotTime {0};
    std::vector<uintptr_t> pcVec;
};

struct SampledFrame {
    uint32_t indent {0};               // the indent of this frame.
    int32_t count {0};                 // the count of this frame in the serial stacks.
    uint32_t level {0};                // the level of this frame in its stack call chain.
    uintptr_t pc {0};                  // pc of this stack frame.
    bool isLeaf {false};               // whether is leaf node in the tree format stack string.
    std::vector<uint64_t> timestamps;  // timestamps of this stack sampled.

    friend std::ostream& operator<<(std::ostream& os, const SampledFrame& frame);
    friend std::istream& operator>>(std::istream& is, SampledFrame& frame);
};

class StackPrinter final {
public:
    StackPrinter();
    StackPrinter(const StackPrinter& other) = delete;
    StackPrinter& operator=(const StackPrinter& other) = delete;

    /**
     * @brief Initialize the unique_stack_table, which used to aggregation the stack pcs.
     *
     * @param pid the pid of the process.
     * @param size the size of unique_stack_table to be initialized.
     * @param name the name of unique_stack_table to be initialized, default unique_stack_table.
     * @return return true if success, otherwise return false.
     */
    bool InitUniqueTable(pid_t pid, uint32_t size, std::string name = "unique_stack_table");

    /**
     * @brief Put the pcs into unique_stack_table to generate stackId.
     *
     * @param pcs the vector of the sampled stack pcs.
     * @param tid the tid of the sampled thread.
     * @param snapshotTime the timestamp of the stack sampled.
     * @return return true if success, otherwise return false.
     */
    bool PutPcsInTable(const std::vector<uintptr_t>& pcs, int tid, uint64_t snapshotTime);

    /**
     * @brief Set the unwind info to the StackPrinter.
     *
     * @param unwinder the shared_ptr of unwinder.
     * @param maps the shared_ptr of process maps.
     * @return void.
     */
    void SetUnwindInfo(const std::shared_ptr<Unwinder>& unwinder, const std::shared_ptr<DfxMaps>& maps);

    /**
     * @brief Get the sampled stack string listed by time order.
     *
     * @param timeStampedPcsVec the vector of sampled stack pcs with its timestamp.
     * @return the string of the sampled stack, listed by time order.
     */
    std::string GetFullStack(const std::vector<TimeStampedPcs>& timeStampedPcsVec);

    /**
     * @brief Get the SampledFrames with tids into map, which can be serialize to bytes and deserialize back to map.
     *
     * @param beginTime the begin time of the time interval to filter the sampled stack, default 0.
     * @param endTime the end time of the time interval to filter sampled stack, default 0.
     * @return the map of the SampledFrame vector of each tid.
     */
    std::map<int, std::vector<SampledFrame>> GetThreadSampledFrames(uint64_t beginTime = 0, uint64_t endTime = 0);

    /**
     * @brief the stack string of the sampled stack in tree format.
     *
     * @param tid the tid of sampled thread.
     * @param printTimes whether to print the timestamps of the stack, default false.
     * @param beginTime the begin time of the time interval to filter the sampled stack, default 0.
     * @param endTime the end time of the time interval to filter sampled stack, default 0.
     * @return the string of the sampled stack, formated in tree style.
     */
    std::string GetTreeStack(int tid, bool printTimes = false, uint64_t beginTime = 0, uint64_t endTime = 0);

    /**
     * @brief Get the heaviest stack string of the tree format stack.
     *
     * @param tid the tid of the sampled thread.
     * @param beginTime the begin time of the time interval to filter the sampled stack, default 0.
     * @param endTime the end time of the time interval to filter sampled stack, default 0.
     * @return the string of the heaviest sampled stack.
     */
    std::string GetHeaviestStack(int tid, uint64_t beginTime = 0, uint64_t endTime = 0);

    /**
     * @brief Print the sampled stack to tree format string from the vector of struct SampledFrame.
     *
     * @param sampledFrameVec the SampledFrame vector of sampled stack.
     * @param printTimes whether to print the timestamps of the stack, default false.
     * @param unwinder the unwinder to unwind stacks.
     * @param maps the maps of the process.
     * @return the string of the sampled stack, formated in tree style.
     */
    static std::string PrintTreeStackBySampledStack(const std::vector<SampledFrame>& sampledFrameVec, bool printTimes,
                                                    const std::shared_ptr<Unwinder>& unwinder,
                                                    const std::shared_ptr<DfxMaps>& maps);

    /**
     * @brief Serialize the sampled stack frames of each tid map by ostream.
     *
     * @param sampledFrameMap the map of the SampledFrame vector of each tid.
     * @param os the ostream to do serialize.
     * @return void.
     */
    static void SerializeSampledFrameMap(const std::map<int, std::vector<SampledFrame>>& sampledFrameMap,
                                         std::ostream& os);

    /**
     * @brief Deserialize the sampled stack frames of tid to map by istream.
     *
     * @param is the istream to do deserialize
     * @return the map of the SampledFrame vector of each tid.
     */
    static std::map<int, std::vector<SampledFrame>> DeserializeSampledFrameMap(std::istream& is);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};
}  // end of namespace HiviewDFX
}  // end of namespace OHOS
#endif
