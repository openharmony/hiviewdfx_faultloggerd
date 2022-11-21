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

#include <libunwind.h>
#include <libunwind_i-ohos.h>
#include <map_info.h>
#include <securec.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include "dfx_log.h"
#include "dfx_signal_handler.h"
#include "faultloggerd_client.h"

#define MAX_FRAME 64
#define BUF_SZ 512

#if defined(__arm__)
typedef enum ARM_REG {
    ARM_R0 = 0,
    ARM_R1,
    ARM_R2,
    ARM_R3,
    ARM_R4,
    ARM_R5,
    ARM_R6,
    ARM_R7,
    ARM_R8,
    ARM_R9,
    ARM_R10,
    ARM_FP,
    ARM_IP,
    ARM_SP,
    ARM_LR,
    ARM_PC
} ARM_REG;
#endif

__attribute__((noinline)) int RequestOutputLogFile(const struct ProcessDumpRequest* request)
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
    DfxLogError(buf);
    if (fd > 0) {
        (void)write(fd, buf, strlen(buf));
    }
}

__attribute__((noinline)) void CrashLocalUnwind(const int fd, const ucontext_t* uc)
{
    unw_cursor_t* cursor = NULL;
    unw_context_t* context = NULL;
    GET_MEMORY(cursor, sizeof(unw_cursor_t));
    GET_MEMORY(context, sizeof(unw_context_t));
    if (!context || !cursor) {
        PrintLog(fd, "Fail to mmap, errno(%d).", errno);
        goto out;
    }

#if defined(__arm__)
    context->regs[ARM_R0] = uc->uc_mcontext.arm_r0;
    context->regs[ARM_R1] = uc->uc_mcontext.arm_r1;
    context->regs[ARM_R2] = uc->uc_mcontext.arm_r2;
    context->regs[ARM_R3] = uc->uc_mcontext.arm_r3;
    context->regs[ARM_R4] = uc->uc_mcontext.arm_r4;
    context->regs[ARM_R5] = uc->uc_mcontext.arm_r5;
    context->regs[ARM_R6] = uc->uc_mcontext.arm_r6;
    context->regs[ARM_R7] = uc->uc_mcontext.arm_r7;
    context->regs[ARM_R8] = uc->uc_mcontext.arm_r8;
    context->regs[ARM_R9] = uc->uc_mcontext.arm_r9;
    context->regs[ARM_R10] = uc->uc_mcontext.arm_r10;
    context->regs[ARM_FP] = uc->uc_mcontext.arm_fp;
    context->regs[ARM_IP] = uc->uc_mcontext.arm_ip;
    context->regs[ARM_SP] = uc->uc_mcontext.arm_sp;
    context->regs[ARM_LR] = uc->uc_mcontext.arm_lr;
    context->regs[ARM_PC] = uc->uc_mcontext.arm_pc;
#else
    // the ucontext.uc_mcontext.__reserved of libunwind is simplified with the system's own in aarch64
    if (memcpy_s(context, sizeof(unw_context_t), uc, sizeof(unw_context_t)) != 0) {
        PrintLog(fd, "memcpy_s context error.");
        goto out;
    }
#endif

    if (unw_init_local(cursor, context) != 0) {
        PrintLog(fd, "Fail to init local unwind context.\n");
        goto out;
    }

    unw_word_t pc;
    unw_word_t sp;
    unw_word_t relPc;
    unw_word_t offset;
    unw_word_t sz;
    unw_word_t prevPc;
    size_t index = 0;
    while (true) {
        if (index > MAX_FRAME) {
            PrintLog(fd, "reach max unwind frame count, stop.\n");
            break;
        }

        if (unw_get_reg(cursor, UNW_REG_IP, (unw_word_t*)(&pc))) {
            PrintLog(fd, "Failed to get current pc, stop.\n");
            break;
        }

        if (index > 1 && prevPc == pc) {
            PrintLog(fd, "repeated pc(%p), stop.\n", pc);
            break;
        }
        prevPc = pc;

        relPc = unw_get_rel_pc(cursor);
        struct map_info* mapInfo = unw_get_map(cursor);
        if (mapInfo == NULL && index > 1) {
            PrintLog(fd, "Invalid frame for pc(%p).\n", relPc);
        }

        sz = unw_get_previous_instr_sz(cursor);
        if (index > 0 && relPc != 0) {
            relPc -= sz;
        }

        char symbol[BUF_SZ] = {0};
        memset_s(&symbol, sizeof(symbol), 0, sizeof(symbol));
        if (unw_get_proc_name(cursor, symbol, sizeof(symbol), (unw_word_t*)(&offset)) == 0) {
            PrintLog(fd, "#%02d %016p %s(%s+%lu)\n", index, relPc,
                mapInfo == NULL ? "Unknown" : mapInfo->path,
                symbol, offset);
        } else {
            PrintLog(fd, "#%02d %016p %s\n", index, relPc,
                mapInfo == NULL ? "Unknown" : mapInfo->path);
        }
        index++;

        int ret = unw_step(cursor);
        if (ret < 0) {
            PrintLog(fd, "Unwind step stop, reason:%d.\n", ret);
            break;
        } else if (ret == 0) {
            DfxLogInfo("Unwind step finish\n");
            break;
        }
    }
out:
    if (cursor != NULL) {
        munmap(cursor, sizeof(unw_cursor_t));
    }
    if (context != NULL) {
        munmap(context, sizeof(unw_context_t));
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

void CrashLocalHandlerFd(const int fd, const struct ProcessDumpRequest* request)
{
    PrintLog(fd, "Pid:%d\n", request->pid);
    PrintLog(fd, "Uid:%d\n", request->uid);
    PrintLog(fd, "Process name:%s\n", request->processName);
    PrintLog(fd, "Reason:Signal(%d)\n", request->siginfo.si_signo);
    PrintLog(fd, "Tid:%d, Name:%s\n", request->tid, request->threadName);
    
    CrashLocalUnwind(fd, &(request->context));
}