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

#include "dfx_unwind_remote.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <link.h>
#include <securec.h>

#include "crash_exception.h"
#include "dfx_unwind_async_thread.h"
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_frame_formatter.h"
#include "dfx_logger.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_symbols.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "process_dumper.h"
#include "printer.h"

namespace OHOS {
namespace HiviewDFX {

DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                    std::shared_ptr<Unwinder> unwinder)
{
    bool ret = false;
    if (process == nullptr || unwinder == nullptr) {
        DFXLOG_WARN("%s::process or unwinder is not initialized.", __func__);
        return ret;
    }

    SetCrashProcInfo(process->processInfo_.processName, process->processInfo_.pid,
                     process->processInfo_.uid);
    UnwindKeyThread(request, process, unwinder);
    UnwindOtherThread(process, unwinder);
    ret = true;
    if (ProcessDumper::GetInstance().IsCrash()) {
        if (process->vmThread_ == nullptr) {
            DFXLOG_WARN("%s::unwind thread is not initialized.", __func__);
        } else {
            Printer::PrintThreadFaultStackByConfig(process, process->vmThread_, unwinder);
        }
        Printer::PrintProcessMapsByConfig(unwinder->GetMaps());
        Printer::PrintThreadOpenFiles(process);
    }

    return ret;
}

void DfxUnwindRemote::UnwindKeyThread(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                      std::shared_ptr<Unwinder> unwinder)
{
    std::shared_ptr<DfxThread> unwThread = process->keyThread_;
    if (ProcessDumper::GetInstance().IsCrash() && (process->vmThread_ != nullptr)) {
        unwThread = process->vmThread_;
    }
    if (unwThread == nullptr) {
        DFXLOG_WARN("%s::unwind thread is not initialized.", __func__);
        return;
    }
    auto unwindAsyncThread = std::make_shared<DfxUnwindAsyncThread>(unwThread, unwinder, request->stackId);
    unwindAsyncThread->UnwindStack();

    std::string fatalMsg = process->GetFatalMessage() + unwindAsyncThread->tip;
    if (!unwindAsyncThread->tip.empty()) {
        if (ProcessDumper::GetInstance().IsCrash()) {
            ReportCrashException(process->processInfo_.processName, process->processInfo_.pid,
                                 process->processInfo_.uid, GetTimeMillisec(),
                                 CrashExceptionCode::CRASH_UNWIND_ESTACK);
        }
    }
    process->SetFatalMessage(fatalMsg);
    Printer::PrintDumpHeader(request, process, unwinder);
    Printer::PrintThreadHeaderByConfig(process->keyThread_);
    Printer::PrintThreadBacktraceByConfig(unwThread);
    if (ProcessDumper::GetInstance().IsCrash()) {
        // Registers of unwThread has been changed, we should print regs from request context.
        Printer::PrintRegsByConfig(DfxRegs::CreateFromUcontext(request->context));
    }
}

void DfxUnwindRemote::UnwindOtherThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<Unwinder> unwinder)
{
    if (!DfxConfig::GetConfig().dumpOtherThreads) {
        return;
    }
    auto threads = process->GetOtherThreads();
    if (threads.empty()) {
        return;
    }

    size_t index = 0;
    for (auto thread : threads) {
        if ((index == 0) && ProcessDumper::GetInstance().IsCrash()) {
            Printer::PrintOtherThreadHeaderByConfig();
        }

        if (thread->Attach()) {
            Printer::PrintThreadHeaderByConfig(thread);
            unwinder->UnwindRemote(thread->threadInfo_.nsTid, false, DfxConfig::GetConfig().maxFrameNums);
            thread->Detach();
            thread->SetFrames(unwinder->GetFrames());
            if (ProcessDumper::GetInstance().IsCrash()) {
                ReportUnwinderException(unwinder->GetLastErrorCode());
            }
            DFXLOG_INFO("%s, unwind tid(%d) finish.", __func__, thread->threadInfo_.nsTid);
            Printer::PrintThreadBacktraceByConfig(thread);
        }
        index++;
    }
}
} // namespace HiviewDFX
} // namespace OHOS
