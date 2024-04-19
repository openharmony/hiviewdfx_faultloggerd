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

static __attribute__((noinline)) bool PrintMaps(const int fd)
{
    bool ret = false;
    FILE *file = fopen(PROC_SELF_MAPS_PATH, "r");
    if (file == NULL) {
        DFXLOG_WARN("Fail to open maps info.");
        return ret;
    }

    char mapInfo[MAPINFO_SIZE] = {0};
    int pos = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    char perms[5] = {0}; // 5:rwxp
    char path[MAPINFO_SIZE] = {0};
    char buf[BUF_SZ] = {0};

    PrintLog(fd, "\nMaps:\n");
    while (fgets(mapInfo, sizeof(mapInfo), file) != NULL) {
        (void)memset_s(&perms, sizeof(perms), 0, sizeof(perms));
        if (sscanf_s(mapInfo, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
            &perms, sizeof(perms), &offset, &pos) != 4) { // 4:scan size
            DFXLOG_WARN("Fail to parse maps info.");
            break;
        }
        (void)memset_s(&path, sizeof(path), 0, sizeof(path));
        if (!TrimAndDupStr(&mapInfo[pos], path)) {
            DFXLOG_ERROR("TrimAndDupStr failed, line: %d.", __LINE__);
            break;
        }

        (void)memset_s(&buf, sizeof(buf), 0, sizeof(buf));
        if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%" PRIx64 "-%" PRIx64 " %s %08" PRIx64 " %s", \
            begin, end, perms, offset, path) <= 0) {
            DFXLOG_ERROR("snprintf_s failed, line: %d.", __LINE__);
            break;
        }
        PrintLog(fd, "%s\n", buf);
    }
    ret = fclose(file);
    return ret;
}

static std::string LocalUnwinderPrint(const ucontext_t* context)
{
    OHOS::HiviewDFX::Unwinder unwind;
    unwind.UnwindLocalWithContext(*context);
    auto regs = OHOS::HiviewDFX::DfxRegs::CreateFromUcontext(*context);
    std::string logContext = unwind.GetFramesStr(unwind.GetFrames()) + regs->PrintRegs() ;
    return logContext;
}

static __attribute__((noinline)) void CrashLocalUnwind(const int fd, const ucontext_t* uc)
{
    if (uc == nullptr) {
        return;
    }
    std::string stackInfo = LocalUnwinderPrint(uc);
    for (unsigned int i = 0; i < stackInfo.length(); i += BUF_SZ_SMALL) {
        PrintLog(fd, "%s", stackInfo.substr(i, BUF_SZ_SMALL).c_str());
    }
    (void)PrintMaps(fd);
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
    exception.time = static_cast<int32_t>(GetTimeMilliseconds());
    if (strncpy_s(exception.message, sizeof(exception.message) - 1, logPath, strlen(logPath)) != 0) {
        DFXLOG_ERROR("strcpy exception msg fail\n");
        return;
    }
    ReportException(exception);
}

void CrashLocalHandlerFd(const int fd, const struct ProcessDumpRequest* request)
{
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
