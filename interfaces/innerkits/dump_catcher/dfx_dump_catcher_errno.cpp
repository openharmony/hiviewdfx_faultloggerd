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
#include <algorithm>
namespace OHOS {
namespace HiviewDFX {
struct DumpCatchErrInfo {
    int32_t code;
    const char *info;
};

const DumpCatchErrInfo ERROR_CODE_MAPS[] = {
    { DUMPCATCH_ESUCCESS, "success" },
    { DUMPCATCH_EPARAM, "param error" },
    { DUMPCATCH_NO_PROCESS, "process has exited" },
    { DUMPCATCH_IS_DUMPING, "process is dumping" },
    { DUMPCATCH_EPERMISSION, "check permission error" },
    { DUMPCATCH_HAS_CRASHED, "process has been crashed" },
    { DUMPCATCH_ECONNECT, "failed to connect to faultloggerd" },
    { DUMPCATCH_EWRITE, "failed to write data to faultloggerd" },
    { DUMPCATCH_EFD, "buf or res fd error" },
    { DUMPCATCH_EPOLL, "poll error" },
    { DUMPCATCH_DUMP_EPTRACE, "failed to ptrace thread" },
    { DUMPCATCH_DUMP_EUNWIND, "failed to unwind" },
    { DUMPCATCH_DUMP_EMAP, "failed to find map" },
    { DUMPCATCH_TIMEOUT_SIGNAL_BLOCK, "signal has been block by target process" },
    { DUMPCATCH_TIMEOUT_KERNEL_FROZEN, "target process has been frozen in kernel" },
    { DUMPCATCH_TIMEOUT_PROCESS_KILLED, "target process has been killed during dumping" },
    { DUMPCATCH_TIMEOUT_DUMP_SLOW, "failed to fully dump due to timeout" },
    { DUMPCATCH_TIMEOUT_DUMP_IN_SLOWPERIOD, "in dump slow period and return kernel stack" },
    { DUMPCATCH_KERNELSTACK_ECREATE, "kernelstack fail due to create hstackval fail" },
    { DUMPCATCH_KERNELSTACK_EOPEN, "kernelstack fail due to open bbox fail" },
    { DUMPCATCH_KERNELSTACK_EIOCTL, "kernelstack fail due to ioctl fail" },
    { DUMPCATCH_KERNELSTACK_TIMEOUT, "kernelstack fail due to wait timeout" },
    { DUMPCATCH_KERNELSTACK_OVER_LIMITL, "kernelstack fail due to over limit" },
    { DUMPCATCH_KERNELSTACK_NONEED, "no need to dump kernelstack" },
    { DUMPCATCH_DUMP_ESYMBOL_NO_PARSE, "no enough time to parse symbol" },
    { DUMPCATCH_DUMP_ESYMBOL_PARSE_TIMEOUT, "parse symbol timeout" },
    { DUMPCATCH_DUMP_SELF_FAIL, "dump self fail" },
    { DUMPCATCH_UNKNOWN, "unknown reason" }
};

std::string DfxDumpCatchError::ToString(const int32_t res)
{
    auto iter = std::find_if(std::begin(ERROR_CODE_MAPS), std::end(ERROR_CODE_MAPS),
        [res](const DumpCatchErrInfo &errInfo) { return errInfo.code == res; });
    return iter != std::end(ERROR_CODE_MAPS) ? iter->info : "invalid error code";
}
} // namespace HiviewDFX
} // namespace OHOS
