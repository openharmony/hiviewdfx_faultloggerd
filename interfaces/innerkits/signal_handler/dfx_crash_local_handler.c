/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <signal.h>
#include <unistd.h>

#include <sys/ucontext.h>

#include <hilog_base/log_base.h>
#include <libunwind_i-ohos.h>
#include <libunwind.h>
#include <map_info.h>
#include <securec.h>

#include "dfx_signal_handler.h"
#include "faultloggerd_client.h"

#define MAX_FRAME 64
#define BUF_SZ 512

__attribute__((noinline)) int RequestOutputLogFile(struct ProcessDumpRequest* request)
{
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        return -1;
    }

    faultloggerdRequest.type = (int32_t)CPP_CRASH;
    faultloggerdRequest.pid = request->pid;
    faultloggerdRequest.tid = request->tid;
    faultloggerdRequest.uid = request->uid;
    faultloggerdRequest.time = request->timeStamp + 1;
    if (strncpy_s(faultloggerdRequest.module, sizeof(faultloggerdRequest.module),
        request->processName, sizeof(faultloggerdRequest.module) - 1) != 0) {
        return -1;
    }

    return RequestFileDescriptorEx(&faultloggerdRequest);
}

__attribute__((noinline)) void PrintLog(int fd, const char *format, ...)
{
    char buf[BUF_SZ] = {0};
    (void)memset_s(&buf, sizeof(buf), 0, sizeof(buf));
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
    HILOG_BASE_ERROR(LOG_CORE, "%{public}s", buf);
    if (fd > 0) {
        (void)write(fd, buf, strlen(buf));
    }
}

__attribute__((noinline)) void CrashLocalUnwind(int fd, ucontext_t* ucontext)
{
    size_t index = 0;
    unw_cursor_t cursor;
    if (unw_init_local(&cursor, (unw_context_t*)ucontext) != 0) {
        PrintLog(fd, "Fail to init local unwind context.\n");
        return;
    }

    unw_word_t pc;
    unw_word_t sp;
    unw_word_t relPc;
    unw_word_t offset;
    unw_word_t sz;
    unw_word_t prevPc;
    struct map_info* mapInfo;
    while (true) {
        if (index > MAX_FRAME) {
            PrintLog(fd, "reach max unwind frame count, stop.\n");
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&pc))) {
            PrintLog(fd, "Failed to get current pc, stop.\n");
            break;
        }

        if (index > 1 && prevPc == pc) {
            PrintLog(fd, "repeated pc, stop.\n");
            break;
        }
        prevPc = pc;

        relPc = unw_get_rel_pc(&cursor);
        mapInfo = unw_get_map(&cursor);
        if (mapInfo == NULL && index > 1) {
            PrintLog(fd, "Invalid frame for pc(%p), stop.\n", relPc);
            break;
        }

        sz = unw_get_previous_instr_sz(&cursor);
        if (index != 0 && relPc != 0) {
            relPc -= sz;
        }

        char g_symbol[BUF_SZ] = {0};
        memset_s(&g_symbol, sizeof(g_symbol), 0, sizeof(g_symbol));
        if (unw_get_proc_name(&cursor, g_symbol, sizeof(g_symbol), (unw_word_t*)(&offset)) == 0) {
            PrintLog(fd, "#%02d %016p %s(%s+%lu)\n", index, relPc,
                mapInfo == NULL ? "Unknown" : mapInfo->path,
                g_symbol, offset);
        } else {
            PrintLog(fd, "#%02d %016p %s(%s+%lu)\n", index, relPc,
                mapInfo == NULL ? "Unknown" : mapInfo->path);
        }
        index++;

        int ret = unw_step(&cursor);
        if (ret <= 0) {
            PrintLog(fd, "Step stop, reason:%d.\n", ret);
            break;
        }
    }

    if (fd >= 0) {
        close(fd);
    }
}

// currently, only stacktrace is logged to faultloggerd
void CrashLocalHandler(struct ProcessDumpRequest* request, siginfo_t* info, ucontext_t* ucontext)
{
    int fd = RequestOutputLogFile(request);
    CrashLocalHandlerFd(fd, request, info, ucontext);
}

void CrashLocalHandlerFd(int fd, struct ProcessDumpRequest* request, siginfo_t* info, ucontext_t* ucontext)
{
    PrintLog(fd, "Pid:%d\n", request->pid);
    PrintLog(fd, "Uid:%d\n", request->uid);
    PrintLog(fd, "Process name:%s\n", request->processName);
    PrintLog(fd, "Reason:Signal(%d)\n", info->si_signo);
    PrintLog(fd, "Tid:%d, Name:%s\n", request->tid, request->threadName);
    
    CrashLocalUnwind(fd, ucontext);
}