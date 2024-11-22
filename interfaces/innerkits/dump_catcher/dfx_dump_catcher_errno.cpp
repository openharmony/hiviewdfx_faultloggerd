/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "dfx_dump_catcher_errno.h"

namespace OHOS {
namespace HiviewDFX {

const std::map<int32_t, std::string> ERROR_CODE_MAP = {
    { DUMPCATCH_ESUCCESS, std::string("no error") },
    { DUMPCATCH_EPARAM, std::string("param error") },
    { DUMPCATCH_NO_PROCESS, std::string("process has exited") },
    { DUMPCATCH_IS_DUMPING, std::string("process is dumping") },
    { DUMPCATCH_EPERMISSION, std::string("check permission error") },
    { DUMPCATCH_HAS_CRASHED, std::string("process has been crashed") },
    { DUMPCATCH_ECONNECT, std::string("failed to connect to faultloggerd") },
    { DUMPCATCH_EWRITE, std::string("failed to write data to faultloggerd") },
    { DUMPCATCH_EFD, std::string("buf or res fd error") },
    { DUMPCATCH_EPOLL, std::string("poll error") },
    { DUMPCATCH_DUMP_EPTRACE, std::string("failed to ptrace thread") },
    { DUMPCATCH_DUMP_EUNWIND, std::string("failed to unwind") },
    { DUMPCATCH_DUMP_EMAP, std::string("failed to find map") },
    { DUMPCATCH_TIMEOUT_SIGNAL_BLOCK, std::string("signal has been block by target process") },
    { DUMPCATCH_TIMEOUT_KERNEL_FROZEN, std::string("target process has been frozen in kernel") },
    { DUMPCATCH_TIMEOUT_PROCESS_KILLED, std::string("target process has been killed during dumping") },
    { DUMPCATCH_TIMEOUT_DUMP_SLOW, std::string("failed to fully dump due to timeout") },
    { DUMPCATCH_KERNELSTACK_ECREATE, std::string("kernelstack fail due to create hstackval fail") },
    { DUMPCATCH_KERNELSTACK_EOPEN, std::string("kernelstack fail due to open bbox fail") },
    { DUMPCATCH_KERNELSTACK_EIOCTL, std::string("kernelstack fail due to ioctl fail") },
    { DUMPCATCH_UNKNOWN, std::string("unknown reason") }
};

std::string DfxDumpCatchError::ToString(const int32_t res)
{
    auto it = ERROR_CODE_MAP.find(res);
    if (it != ERROR_CODE_MAP.end()) {
        return it->second;
    }
    return std::string("invalid error code");
}
} // namespace HiviewDFX
} // namespace OHOS