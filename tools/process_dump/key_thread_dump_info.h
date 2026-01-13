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

#ifndef KEY_THREAD_DUMP_INFO_H
#define KEY_THREAD_DUMP_INFO_H
#include <string>
#include "dfx_process.h"
#include "dfx_dump_request.h"
#include "dump_info.h"
#include "unwinder.h"
#include "dump_info_factory.h"
 
namespace OHOS {
namespace HiviewDFX {
class KeyThreadDumpInfo : public DumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    void Collect(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override {}
    int UnwindStack(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<KeyThreadDumpInfo>(); }
private:
    bool GetKeyThreadStack(DfxProcess& process, Unwinder& unwinder);
    void UnwindThreadByParseStackIfNeed(std::shared_ptr<DfxThread> thread, std::shared_ptr<DfxMaps> maps);
    std::string unwindFailTip_;
};
}
}
#endif