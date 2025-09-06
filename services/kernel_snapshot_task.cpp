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

#include "kernel_snapshot_task.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <string>

#include "dfx_log.h"
#include "parameters.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
std::string ReadKernelSnapshot()
{
#ifdef FAULTLOGGERD_TEST
    constexpr auto kernelKboxSnapshot = "/data/test/resource/testdata/kernel_snapshot_execption.txt";
#else
    constexpr auto kernelKboxSnapshot = "/sys/kbox/snapshot_clear";
#endif
    SmartFd fd(open(kernelKboxSnapshot, O_RDONLY));
    if (!fd) {
        DFXLOGE("open snapshot %{public}s failed %{public}d", kernelKboxSnapshot, errno);
        return "";
    }
    constexpr int buffLength = 1024;
    char buffer[buffLength] = {0};
    std::string snapshotCont;
    ssize_t ret = 0;
    do {
        ret = read(fd.GetFd(), buffer, buffLength - 1);
        if (ret > 0) {
            snapshotCont.append(buffer, static_cast<size_t>(ret));
        }
        if (ret < 0) {
            DFXLOGE("read snapshot failed %{public}d", errno);
        }
    } while (ret > 0);
    return snapshotCont;
}
}

ReadKernelSnapshotTask::ReadKernelSnapshotTask() : TimerTask(true)
{
    constexpr int minInterval = 3;
    constexpr auto kernelSnapshotInterval = "kernel_snapshot_check_interval";
    constexpr int defaultInterval = 60;
    int interval = std::max(system::GetIntParameter(kernelSnapshotInterval, defaultInterval), minInterval);
#ifdef FAULTLOGGERD_TEST
    SetTimeOption(minInterval, interval);
#else
    SetTimeOption(interval, interval);
#endif
}

void ReadKernelSnapshotTask::OnTimer()
{
    const std::string snapshotCont = ReadKernelSnapshot();
    if (snapshotCont.empty()) {
        DFXLOGD("the snapshot file does not exist or is empty.");
        return;
    }
    DFXLOGI("read snapshot begin with %{public}s", snapshotCont.substr(0, 25).c_str()); // 25 : only need 25
    constexpr auto kernelSnapshotLibraryName = "libkernel_snapshot.z.so";
    void* handle = dlopen(kernelSnapshotLibraryName, RTLD_LAZY);
    if (handle == nullptr) {
        DFXLOGE("failed dlopen library %{public}s for error %{public}d", kernelSnapshotLibraryName, errno);
        return;
    }
    constexpr auto methodName = "ProcessKernelSnapShot";
    auto processKernelSnapShot = reinterpret_cast<void (*)(const std::string&)>(dlsym(handle, methodName));
    if (processKernelSnapShot == nullptr) {
        DFXLOGE("can't find method %{public}s in %{public}s, just exit", methodName, kernelSnapshotLibraryName);
        dlclose(handle);
        return;
    }
    processKernelSnapShot(snapshotCont);
    dlclose(handle);
}
}
}