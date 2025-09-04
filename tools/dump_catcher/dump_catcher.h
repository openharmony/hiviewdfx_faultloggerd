/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_DUMP_CATCHER_H
#define DFX_DUMP_CATCHER_H

#include <cinttypes>
#include <string>
#include "nocopyable.h"

namespace OHOS {
namespace HiviewDFX {
enum class DumpType {
    DUMP_USER_STACK,
    DUMP_KERNEL_STACK,
};

struct DumpOptions {
    DumpType type = DumpType::DUMP_USER_STACK;
    int32_t pid = 0;
    int32_t tid = 0;
    int timeout = 3000; // 3000 : default timeout 3000ms in dump user stack
    bool needArk = false;
    bool needParse = false;
};
class DumpCatcher final {
public:
    static DumpCatcher &GetInstance();
    ~DumpCatcher() = default;

    void Dump(const DumpOptions& dumpOpt) const;

    void DumpKernelStack(const DumpOptions& dumpOpt) const;
private:
    DumpCatcher() = default;
    std::string GetBundleName(int32_t pid) const;
    DISALLOW_COPY_AND_MOVE(DumpCatcher);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
