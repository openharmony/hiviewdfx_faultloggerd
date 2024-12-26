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

#include "fault_logger_daemon.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_dump_request.h"
#include "dfx_signal_local_handler.h"
#include "dfx_util.h"
#include "temp_file_manager.h"

static int DoGetCrashFd(const struct ProcessDumpRequest* request)
{
    if (request == nullptr) {
        return -1;
    }
    return OHOS::HiviewDFX::TempFileManager::CreateFileDescriptor(FaultLoggerType::CPP_CRASH,
                                                                  request->pid, request->tid, request->timeStamp);
}
#endif

int main(int argc, char *argv[])
{
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_GetCrashFdFunc(DoGetCrashFd);
    DFX_InstallLocalSignalHandler();
#endif
    auto& faultLoggerDaemon = OHOS::HiviewDFX::FaultLoggerDaemon::GetInstance();
    faultLoggerDaemon.StartServer();
    return 0;
}
