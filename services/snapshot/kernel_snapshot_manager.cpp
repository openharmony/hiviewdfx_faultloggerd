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

#include "kernel_snapshot_manager.h"

#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>

#include "parameters.h"

#include "dfx_log.h"
#include "dfx_util.h"

#include "kernel_snapshot_content_builder.h"
#include "kernel_snapshot_processor_impl.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr const char * const KERNEL_KBOX_SNAPSHOT = "/sys/kbox/snapshot_clear";
constexpr const char * const KERNEL_SNAPSHOT_INTERVAL = "kernel_snapshot_check_interval";
constexpr int BUFFER_LEN = 1024;
constexpr int DEFAULT_INTERVAL = 60;
constexpr int MIN_INTERVAL = 3;
}

int KernelSnapshotManager::GetSnapshotCheckInterval()
{
    static int value = OHOS::system::GetIntParameter(KERNEL_SNAPSHOT_INTERVAL, DEFAULT_INTERVAL);
    value = std::max(value, MIN_INTERVAL);
    DFXLOGI("monitor crash kernel snapshot interval %{public}d", value);
    return value;
}

std::string KernelSnapshotManager::ReadKernelSnapshot()
{
    SmartFd snapshotFd(open(KERNEL_KBOX_SNAPSHOT, O_RDONLY));
    if (!snapshotFd) {
        DFXLOGE("open snapshot filed %{public}d", errno);
        return "";
    }

    char buffer[BUFFER_LEN] = {0};
    std::string snapshotCont;
    ssize_t ret = 0;
    do {
        ret = read(snapshotFd.GetFd(), buffer, BUFFER_LEN - 1);
        if (ret > 0) {
            snapshotCont.append(buffer, static_cast<size_t>(ret));
        }
        if (ret < 0) {
            DFXLOGE("read snapshot filed %{public}d", errno);
        }
    } while (ret > 0);
    return snapshotCont;
}

void KernelSnapshotManager::MonitorCrashKernelSnapshot()
{
    DFXLOGI("enter %{public}s ", __func__);
    pthread_setname_np(pthread_self(), "KernelSnapshot");
    int interval = GetSnapshotCheckInterval();

    std::unique_ptr<IKernelSnapshotProcessor> processor(new KernelSnapshotProcessorImpl());

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        if (access(KERNEL_KBOX_SNAPSHOT, F_OK) < 0) {
            DFXLOGE("can't find %{public}s, just exit", KERNEL_KBOX_SNAPSHOT);
            break;
        }
        std::string snapshotCont = ReadKernelSnapshot();
        processor->Process(snapshotCont);
    }
}

void KernelSnapshotManager::StartMonitor()
{
    DFXLOGI("monitor kernel crash snapshot start!");
    if (!IsBetaVersion()) {
        DFXLOGW("monitor kernel crash snapshot func not support");
        return;
    }
    std::thread catchThread = std::thread([] {
        KernelSnapshotManager kernelSnapshotManager;
        kernelSnapshotManager.MonitorCrashKernelSnapshot();
    });
    catchThread.detach();
}
} // namespace HiviewDFX
} // namespace OHOS
