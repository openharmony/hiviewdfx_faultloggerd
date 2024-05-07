/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#include "dfx_crash_local_handler.h"

#include <securec.h>
#include <csignal>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <cinttypes>
#include <unistd.h>
#include <pthread.h>
#include <cerrno>
#include "dfx_log.h"
#include "dfx_cutil.h"
#include "dfx_signalhandler_exception.h"
#include "faultloggerd_client.h"
#include "unwinder.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxCrashLocalHandler"
#endif

#define MAX_FRAME 64
#define BUF_SZ 512
#define MAPINFO_SIZE 256
#define TIME_DIV 1000
#define BUF_SZ_SMALL 256

static __attribute__((noinline)) int RequestOutputLogFile(const struct ProcessDumpRequest* request)
{
    struct FaultLoggerdRequest faultloggerdRequest;
    (void)memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest));

    faultloggerdRequest.type = (int32_t)CPP_CRASH;
    faultloggerdRequest.pid = request->pid;
    faultloggerdRequest.tid = request->tid;
    faultloggerdRequest.uid = request->uid;
    faultloggerdRequest.time = request->timeStamp + 1;
    return RequestFileDescriptorEx(&faultloggerdRequest);
}

static __attribute__((noinline)) void PrintLog(int fd, const char *format, ...)
{
    char buf[BUF_SZ] = {0};
    va_list args;
    va_start(args, format);
    int size = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    va_end(args);
    if (size == -1) {
        if (fd > 0) {
            const char* error = "PrintLog vsnprintf_s fail\n";
            (void)write(fd, error, strlen(error));
        }
        return;
    }
    DFXLOG_ERROR("%s", buf);
    if (fd > 0) {
        (void)write(fd, buf, strlen(buf));
    }
}

__attribute__((noinline)) void CrashLocalUnwind(const int fd, const ucontext_t* uc)
{
    if (uc == nullptr) {
        return;
    }
    OHOS::HiviewDFX::Unwinder unwind;
    unwind.UnwindLocalWithContext(*uc);
    auto regs = OHOS::HiviewDFX::DfxRegs::CreateFromUcontext(*uc);
    std::string logContext = unwind.GetFramesStr(unwind.GetFrames()) + regs->PrintRegs();
    logContext.append("\nMaps:\n");
    for (const auto &map : unwind.GetMaps()->GetMaps()) {
        logContext.append(map->ToString());
    }

    for (unsigned int i = 0; i < logContext.length(); i += BUF_SZ_SMALL) {
        PrintLog(fd, "%s", logContext.substr(i, BUF_SZ_SMALL).c_str());
    }

    if (fd >= 0) {
        close(fd);
    }
}

// currently, only stacktrace is logged to faultloggerd
void CrashLocalHandler(const struct ProcessDumpRequest* request)
{
    int fd = RequestOutputLogFile(request);
    CrashLocalHandlerFd(fd, request);
}

static void PrintTimeStamp(const int fd)
{
    uint64_t currentTime = GetTimeMilliseconds();
    char secBuf[BUF_SZ] = {0};
    char printBuf[BUF_SZ] = {0};
    time_t sec = static_cast<time_t>(currentTime / TIME_DIV);
    uint64_t millisec = currentTime % TIME_DIV;
    struct tm* t = localtime(&sec);
    if (!t) {
        return;
    }
    (void)strftime(secBuf, sizeof(secBuf) - 1, "%Y-%m-%d %H:%M:%S", t);
    if (snprintf_s(printBuf, sizeof(printBuf), sizeof(printBuf) - 1,
            "%s.%03u\n", secBuf, millisec) < 0) {
        DFXLOG_ERROR("snprintf timestamp fail\n");
        return;
    }
    PrintLog(fd, "Timestamp:%s", printBuf);
}

static void ReportToHiview(const char* logPath, const struct ProcessDumpRequest* request)
{
    struct CrashDumpException exception;
    (void)memset_s(&exception, sizeof(struct CrashDumpException), 0, sizeof(struct CrashDumpException));
    exception.pid = request->pid;
    exception.uid = request->uid;
    exception.error = CRASH_DUMP_LOCAL_REPORT;
    exception.time = static_cast<int64_t>(GetTimeMilliseconds());
    if (strncpy_s(exception.message, sizeof(exception.message) - 1, logPath, strlen(logPath)) != 0) {
        DFXLOG_ERROR("strcpy exception msg fail\n");
        return;
    }
    ReportException(exception);
}

void CrashLocalHandlerFd(const int fd, const struct ProcessDumpRequest* request)
{
    if (request == nullptr) {
        return;
    }
    PrintTimeStamp(fd);
    PrintLog(fd, "Pid:%d\n", request->pid);
    PrintLog(fd, "Uid:%d\n", request->uid);
    PrintLog(fd, "Process name:%s\n", request->processName);
#if defined(__LP64__)
    PrintLog(fd, "Reason:Signal(%d)@%018p\n", request->siginfo.si_signo, request->siginfo.si_addr);
#else
    PrintLog(fd, "Reason:Signal(%d)@%010p\n", request->siginfo.si_signo, request->siginfo.si_addr);
#endif
    PrintLog(fd, "Fault thread info:\n");
    PrintLog(fd, "Tid:%d, Name:%s\n", request->tid, request->threadName);
    CrashLocalUnwind(fd, &(request->context));
    char logFileName[BUF_SZ] = {0};
    (void)snprintf_s(logFileName, sizeof(logFileName), sizeof(logFileName) - 1,
        "/data/log/faultlog/temp/cppcrash-%d-%llu", request->pid, request->timeStamp + 1);
    ReportToHiview(logFileName, request);
}
