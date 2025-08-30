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

#include "dump_catcher.h"

#include "dfx_define.h"
#include "dfx_dump_catcher.h"
#include "dfx_log.h"
#include "dfx_json_formatter.h"
#include "dfx_kernel_stack.h"
#include "elapsed_time.h"
#include "procinfo.h"
#include "proc_util.h"
#include "dump_utils.h"
#ifndef is_ohos_lite
#include "bundle_mgr_interface.h"
#include "bundle_mgr_proxy.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr int WAIT_GET_KERNEL_STACK_TIMEOUT = 1000; // 1000 : time out 1000 ms

#ifndef is_ohos_lite
sptr<AppExecFwk::IBundleMgr> GetBundleManager()
{
    auto systemManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemManager) {
        DFXLOGE("Get system ability manager failed");
        return nullptr;
    }
    auto remoteObject = systemManager->GetSystemAbility(BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    if (!remoteObject) {
        DFXLOGE("Get system ability failed");
        return nullptr;
    }
    sptr<AppExecFwk::IBundleMgr> bundleMgrProxy = iface_cast<AppExecFwk::IBundleMgr>(remoteObject);
    return bundleMgrProxy;
}
#endif

std::string GetBundleNameByUid(int32_t uid)
{
#ifndef is_ohos_lite
    constexpr long minUid = 10000; // 10000 : minimum uid for hap
    if (uid < minUid) {
        return "";
    }
    auto bundleInstance = GetBundleManager();
    if (bundleInstance == nullptr) {
        DFXLOGE("bundleInstance is nullptr");
        return "";
    }
    std::string bundleName;
    auto ret = bundleInstance->GetNameForUid(uid, bundleName);
    if (ret != ERR_OK) {
        DFXLOGE("GetNameForUid failed! ret = %{public}d", ret);
        return "";
    }
    return bundleName;
#endif
    return "";
}
}
DumpCatcher &DumpCatcher::GetInstance()
{
    static DumpCatcher ins;
    return ins;
}

void DumpCatcher::Dump(const DumpOptions& dumpOpt) const
{
    ElapsedTime counter;
    DfxDumpCatcher dfxDump;
    std::string msg = "";
    int timeout = dumpOpt.timeout > WAIT_GET_KERNEL_STACK_TIMEOUT ? dumpOpt.timeout : WAIT_GET_KERNEL_STACK_TIMEOUT;
    auto dumpResult = dfxDump.DumpCatchWithTimeout(dumpOpt.pid, msg, timeout, dumpOpt.tid);
    auto dumpTime = counter.Elapsed<std::chrono::milliseconds>();
    std::string toFind = "Result:";
    size_t startPos = msg.find(toFind);
    if (startPos != std::string::npos) {
        size_t endPos = msg.find("\n", startPos);
        if (endPos != std::string::npos) {
            msg.erase(startPos, endPos - startPos + 1);
        }
    }
    if (dumpResult.first == -1) {
        printf("Result:dump failed.\n");
    } else if (dumpResult.first == 0) {
        printf("Result:dump normal stack success.\n");
    } else if (dumpResult.first == 1) {
        printf("Result:dump kernel stack success.\n");
    }
    printf("%s", dumpResult.second.c_str());
    printf("%s\n", msg.c_str());
    printf("total cost:(%" PRId64 ")ms\n", dumpTime);
}

void DumpCatcher::DumpKernelStack(const DumpOptions& dumpOpt) const
{
    ElapsedTime counter;
    std::string kernelStackInfo;
    std::function<bool(int)> stackTask = [&kernelStackInfo, &dumpOpt](int tid) {
        if (tid <= 0) {
            return false;
        }
        std::string tidKernelStackInfo;
        auto ret = DfxGetKernelStack(tid, tidKernelStackInfo, dumpOpt.needArk);
        if (ret == 0) {
            kernelStackInfo.append(tidKernelStackInfo);
        }
        return true;
    };
    std::vector<int> tids;
    (void)GetTidsByPidWithFunc(dumpOpt.pid, tids, stackTask);
    auto getKernelStackTime = counter.Elapsed<std::chrono::milliseconds>();
    counter.Reset();
    std::string parsedKernelStack;
    bool ret = false;
    time_t parseSymbolTime = 0;
    if (dumpOpt.needParse) {
        std::string bundleName = GetBundleName(dumpOpt.pid);
        ret = DfxJsonFormatter::FormatKernelStack(kernelStackInfo, parsedKernelStack, false, true, bundleName);
        parseSymbolTime = counter.Elapsed<std::chrono::milliseconds>();
    }
    if (ret) {
        printf("%s\n", parsedKernelStack.c_str());
    } else {
        printf("no need to format kernel stack or format failed.\n");
        printf("%s\n", kernelStackInfo.c_str());
    }
    printf("threads size:(%zu)\n", tids.size());
    printf("get kernel stack cost:(%" PRId64 ")ms\n", getKernelStackTime);
    if (dumpOpt.needParse) {
        printf("parse symbol cost:(%" PRId64 ")ms\n", parseSymbolTime);
    }
}

std::string DumpCatcher::GetBundleName(int32_t pid) const
{
    long uid = 0;
    uint64_t sigBlk = 0;
    if (!GetUidAndSigBlk(pid, uid, sigBlk)) {
        printf("GetUidAndSigBlk failed for pid(%d).\n", pid);
        return "";
    }
    return GetBundleNameByUid(uid);
}
} // namespace HiviewDFX
} // namespace OHOS
