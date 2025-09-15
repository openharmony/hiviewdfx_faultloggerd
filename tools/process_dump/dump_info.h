/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef DUMP_INFO_H
#define DUMP_INFO_H

#include <csignal>
#include <set>
#include <string>
#include "dfx_process.h"
#include "dfx_dump_request.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {

class DumpInfo {
public:
    virtual ~DumpInfo() {}
    virtual void SetDumpInfo(const std::shared_ptr<DumpInfo>& dumpInfo) {}
    virtual void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) = 0;
    virtual int UnwindStack(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) = 0;
    virtual void GetMemoryValues(std::set<uintptr_t>& memoryValues) {};
};
}
}
#endif