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
#ifndef DFX_UNWIND_REMOTE_H
#define DFX_UNWIND_REMOTE_H

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextern-c-compat"
#endif

#include <map>
#include <memory>

#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "nocopyable.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
class DfxUnwindRemote final {
public:
    static DfxUnwindRemote &GetInstance();
    ~DfxUnwindRemote() = default;

    bool UnwindProcess(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                       std::shared_ptr<Unwinder> unwinder, pid_t vmPid = 0);
    bool InitProcessAllThreadRegs(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
        bool isCrash);

private:
    DfxUnwindRemote() = default;
    void UnwindKeyThread(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                      std::shared_ptr<Unwinder> unwinder, pid_t vmPid = 0);
    void UnwindOtherThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<Unwinder> unwinder,
        pid_t vmPid = 0);

    DISALLOW_COPY_AND_MOVE(DfxUnwindRemote);
    bool InitTargetKeyThreadRegs(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process);
    void InitOtherThreadRegs(std::shared_ptr<DfxProcess> process, bool isCrash);
    bool isVmProcAttach = false;
};
}   // namespace HiviewDFX
}   // namespace OHOS

#endif  // DFX_UNWIND_REMOTE_H
