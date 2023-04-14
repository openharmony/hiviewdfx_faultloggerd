/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "rustpanic_reporter.h"

#include <cstring>
#include <securec.h>
#include <unistd.h>

#include "backtrace_local.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "hisysevent.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxPanicReporter"
#define LOG_DOMAIN 0xD002D11
static constexpr char KEY_RUST_PANIC[] = "RUST_PANIC";
static constexpr uint32_t REASON_BUF = 512;
}

bool ReportTraceInfo(RustPanicInfo *info)
{
    char panicReason[REASON_BUF] = {0};
    if (snprintf_s(panicReason, REASON_BUF, REASON_BUF - 1,
        "file:%s line:%d message:%s", info->filename, info->line, info->msg) <= 0) {
        DfxLogError("snprintf_s failed to format panic reason string");
    }

    std::string module;
    std::string threadName;
    ReadProcessName(getpid(), module);
    ReadThreadName(getpid(), threadName);

    std::string threadLabel = "Thread name:" + threadName + "\n";
    std::string trace;
    if (!GetBacktrace(trace)) {
        DfxLogError("Failed to get rust panic thread backtrace.");
        trace = "Failed to unwind rust panic thread.";
    }

    HiSysEventWrite(HiSysEvent::Domain::RELIABILITY, KEY_RUST_PANIC, HiSysEvent::EventType::FAULT,
        "MODULE", module, "REASON", panicReason, "PID", getpid(), "TID", gettid(), "UID", getuid(),
        "SUMMARY", threadLabel + trace, "HAPPEN_TIME", GetTimeMilliSeconds());

    return true;
}

bool PrintTraceInfo()
{
    bool ret = false;
    const int fd = STDOUT_FILENO;
    ret = PrintBacktrace(fd);
    DfxLogInfo("ret: %d", ret);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS