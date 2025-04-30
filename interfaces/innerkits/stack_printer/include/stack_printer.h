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

class StackPrinter final {
public:
    StackPrinter(const std::shared_ptr<Unwinder>& unwinder);
    StackPrinter(const StackPrinter& other) = delete;
    StackPrinter& operator=(const StackPrinter& other) = delete;

    void SetMaps(std::shared_ptr<DfxMaps> maps);
    bool PutPcsInTable(const std::vector<uintptr_t>& pcs, uint64_t snapshotTime);
    std::string GetFullStack(const std::vector<TimeStampedPcs>& timeStampedPcsVec);
    std::string GetTreeStack(bool printTimes = false, uint64_t beginTime = 0, uint64_t endTime = 0);
    std::string GetHeaviestStack(uint64_t beginTime = 0, uint64_t endTime = 0);
    bool InitUniqueTable(pid_t pid, uint32_t size, std::string name = "unique_stack_table");

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};
} // end of namespace HiviewDFX
} // end of namespace OHOS
#endif
