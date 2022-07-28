/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <unistd.h>
#include "fault_logger_daemon.h"
#include "dfx_log.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include <securec.h>
#include "dfx_signal_local_handler.h"
#include "dfx_crash_local_handler.h"
#include "dfx_cutil.h"

static void DFX_SignalHandler(int sig, siginfo_t *si, void *context)
{
    OHOS::HiviewDFX::FaultLoggerDaemon daemon;
    int32_t type = (int32_t)FaultLoggerType::CPP_CRASH;
    int32_t pid = getpid();
    uint64_t time = GetTimeMilliseconds();
    int fd = daemon.CreateFileForRequest(type, pid, time, false);
    if (fd < 0) {
        DfxLogError("%s :: Failed to create log file", __func__);
        return;
    }

    struct ProcessDumpRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    DFX_InitDumpRequest(&request, sig);

    CrashLocalHandlerFd(fd, &request, si, (ucontext *)context);
}
#endif

int main(int argc, char *argv[])
{
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_SetSignalHandlerFunc(DFX_SignalHandler);
    DFX_InstallLocalSignalHandler();
#endif
    OHOS::HiviewDFX::FaultLoggerDaemon daemon;
    daemon.StartServer();
    return 0;
}
