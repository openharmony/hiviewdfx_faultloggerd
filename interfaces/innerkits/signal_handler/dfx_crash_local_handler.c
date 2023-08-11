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

#include <securec.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include "stdbool.h"
#include "errno.h"
#include "dfx_log.h"
#include "dfx_cutil.h"
#include "dfx_signal_handler.h"
#include "faultloggerd_client.h"
#include "libunwind.h"
#include "libunwind_i-ohos.h"

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
static const int BACK_STACK_MAX_STEPS = 64; // max unwind 64 steps.

__attribute__((noinline)) int RequestOutputLogFile(const struct ProcessDumpRequest* request)
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
    DFXLOG_ERROR(buf);
    if (fd > 0) {
        (void)write(fd, buf, strlen(buf));
    }
}

#ifndef __aarch64__
__attribute__((noinline)) bool UnwindWithContext(const int fd, unw_context_t* context)
{
    bool ret = false;
    if (!context) {
        PrintLog(fd, "context is NULL.\n");
        return ret;
    }

    unw_cursor_t* cursor = NULL;
    GET_MEMORY(cursor, sizeof(unw_cursor_t));
    if (!cursor) {
        PrintLog(fd, "Fail to mmap cursor, errno(%d).\n", errno);
        return ret;
    }

    if (unw_init_local(cursor, context) != 0) {
        PrintLog(fd, "Fail to init local unwind context.\n");
        munmap(cursor, sizeof(unw_cursor_t));
        return ret;
    }

    unw_word_t pc;
    unw_word_t relPc;
    unw_word_t offset;
    unw_word_t sz;
    unw_word_t prevPc;
    size_t index = 0;
    do {
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
        sz = unw_get_previous_instr_sz(cursor);
        if ((index > 1) && (relPc > sz)) {
            relPc -= sz;
        }

        struct map_info* mapInfo = unw_get_map(cursor);
        if (mapInfo == NULL && index > 1) {
            PrintLog(fd, "Invalid frame for pc(%p).\n", relPc);
        }

        char symbol[BUF_SZ] = {0};
        (void)memset_s(&symbol, sizeof(symbol), 0, sizeof(symbol));
        if (unw_get_proc_name(cursor, symbol, sizeof(symbol), (unw_word_t*)(&offset)) == 0) {
            PrintLog(fd, "#%02d %016p %s(%s+%lu)\n", index, relPc,
                mapInfo == NULL ? "Unknown" : mapInfo->path,
                symbol, offset);
        } else {
            PrintLog(fd, "#%02d %016p %s\n", index, relPc,
                mapInfo == NULL ? "Unknown" : mapInfo->path);
        }
        index++;

        int stepRet = unw_step(cursor);
        if (stepRet < 0) {
            PrintLog(fd, "Unwind step stop, reason:%d\n", ret);
            break;
        } else if (stepRet == 0) {
            DFXLOG_INFO("Unwind step finish");
            ret = true;
            break;
        }
    } while (true);

    munmap(cursor, sizeof(unw_cursor_t));
    return ret;
}

#else
__attribute__((noinline)) bool PrintMaps(const int fd)
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

__attribute__((noinline)) void PrintRegs(const int fd, unw_context_t* context)
{
    PrintLog(fd, "\nRegisters:");
    char buf[BUF_SZ] = {0};

    for (int i = UNW_AARCH64_X0; i < UNW_AARCH64_PSTATE; ++i) {
        if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x%d:%016lx", \
            i, context->uc_mcontext.regs[i]) <= 0) {
            DFXLOG_ERROR("snprintf_s failed, line: %d.", __LINE__);
            break;
        }
        if (i % 4 == 0) { // per 4 will new line
            PrintLog(fd, "\n");
        }
        PrintLog(fd, "%s ", buf);
    }
    PrintLog(fd, "\n");
}

__attribute__((noinline)) bool UnwindWithContextByFramePointer(const int fd, unw_context_t* context)
{
    bool ret = false;
    if (!context) {
        PrintLog(fd, "context is NULL.\n");
        return ret;
    }

    uintptr_t fp = context->uc_mcontext.regs[UNW_AARCH64_X29];
    uintptr_t pc = context->uc_mcontext.pc;

    pthread_attr_t tattr;
    void *base;
    size_t size;
    pthread_getattr_np(pthread_self(), &tattr);
    pthread_attr_getstack(&tattr, &base, &size);
    uintptr_t stackBottom = (uintptr_t)base;
    uintptr_t stackTop = (uintptr_t)base + size;

    int index = 0;
    do {
        uintptr_t prevFp = fp;
        if (stackBottom < prevFp && (prevFp + sizeof(uintptr_t)) < stackTop) {
            PrintLog(fd, "#%02d fp(%lx) pc(%lx)\n", index, fp, pc);
            fp = *(uintptr_t*)prevFp;
            pc = *(uintptr_t*)(prevFp + sizeof(uintptr_t));
        } else {
            DFXLOG_INFO("Unwind step finish");
            break;
        }
        index++;
    } while (index < BACK_STACK_MAX_STEPS);

    PrintRegs(fd, context);
    ret = PrintMaps(fd);
    return ret;
}
#endif

__attribute__((noinline)) void CrashLocalUnwind(const int fd, const ucontext_t* uc)
{
    unw_context_t* context = NULL;
    GET_MEMORY(context, sizeof(unw_context_t));
    if (!context) {
        PrintLog(fd, "Fail to mmap context, errno(%d).\n", errno);
        goto out;
    }

#if defined(__arm__)
    context->regs[UNW_ARM_R0] = uc->uc_mcontext.arm_r0;
    context->regs[UNW_ARM_R1] = uc->uc_mcontext.arm_r1;
    context->regs[UNW_ARM_R2] = uc->uc_mcontext.arm_r2;
    context->regs[UNW_ARM_R3] = uc->uc_mcontext.arm_r3;
    context->regs[UNW_ARM_R4] = uc->uc_mcontext.arm_r4;
    context->regs[UNW_ARM_R5] = uc->uc_mcontext.arm_r5;
    context->regs[UNW_ARM_R6] = uc->uc_mcontext.arm_r6;
    context->regs[UNW_ARM_R7] = uc->uc_mcontext.arm_r7;
    context->regs[UNW_ARM_R8] = uc->uc_mcontext.arm_r8;
    context->regs[UNW_ARM_R9] = uc->uc_mcontext.arm_r9;
    context->regs[UNW_ARM_R10] = uc->uc_mcontext.arm_r10;
    context->regs[UNW_ARM_R11] = uc->uc_mcontext.arm_fp;
    context->regs[UNW_ARM_R12] = uc->uc_mcontext.arm_ip;
    context->regs[UNW_ARM_R13] = uc->uc_mcontext.arm_sp;
    context->regs[UNW_ARM_R14] = uc->uc_mcontext.arm_lr;
    context->regs[UNW_ARM_R15] = uc->uc_mcontext.arm_pc;
#else
    // the ucontext.uc_mcontext.__reserved of libunwind is simplified with the system's own in aarch64
    if (memcpy_s(context, sizeof(unw_context_t), uc, sizeof(unw_context_t)) != 0) {
        PrintLog(fd, "memcpy_s context error.\n");
        goto out;
    }
#endif

#ifdef __aarch64__
    UnwindWithContextByFramePointer(fd, context);
#else
    UnwindWithContext(fd, context);
#endif

out:
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
#if defined(__LP64__)
    PrintLog(fd, "Reason:Signal(%d)@%018p\n", request->siginfo.si_signo, request->siginfo.si_addr);
#else
    PrintLog(fd, "Reason:Signal(%d)@%010p\n", request->siginfo.si_signo, request->siginfo.si_addr);
#endif
    PrintLog(fd, "Tid:%d, Name:%s\n", request->tid, request->threadName);

    CrashLocalUnwind(fd, &(request->context));
}
