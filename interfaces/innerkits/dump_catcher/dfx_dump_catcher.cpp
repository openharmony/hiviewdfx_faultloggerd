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

/* This files contains faultlog sdk interface functions. */

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wextern-c-compat"
#endif

#include "dfx_dump_catcher.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdio>
#include <ctime>
#include <directory_ex.h>
#include <file_ex.h>
#include <fcntl.h>
#include <libunwind.h>
#include <strstream>
#include <pthread.h>
#include <securec.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <string>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ucontext.h>
#include <vector>

#include <hilog/log.h>
#include "../faultloggerd_client/include/faultloggerd_client.h"
#include "dfx_dump_writer.h"
#include "dfx_log.h"
#include "dfx_process.h"
#include "dfx_thread.h"
#include "dfx_util.h"

static const int SYMBOL_BUF_SIZE = 4096;
static const int PID2 = 2;
static const int BACK_STACK_INFO_SIZE = (16 * 1024);  // 16K
char* g_StackInfo_ = nullptr;
static const int BACK_STACK_MAX_STEPS = 64;  // max unwind 64 steps.
static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";
static const std::string DUMP_STACK_TAG_FAILED = "failed:";

namespace OHOS {
namespace HiviewDFX {
DfxDumpCatcher::DfxDumpCatcher()
{
    DfxLogDebug("%s :: construct.", DFXDUMPCATCHER_TAG.c_str());
}

DfxDumpCatcher::~DfxDumpCatcher()
{
    DfxLogDebug("%s :: destructor.", DFXDUMPCATCHER_TAG.c_str());
}

long DfxDumpCatcher::WriteDumpInfo(long current_position, size_t index, std::shared_ptr<DfxFrames> frame)
{
    int writeNumber = 0;
    char* current_pos = g_StackInfo_ + current_position;
    if (frame->GetFrameFuncName() == "") {
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-current_position,
            "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s\n",
            index, frame->GetFrameRelativePc(), frame->GetFramePc(), (frame->GetFrameMap() == nullptr) ?
            "Unknown" : frame->GetFrameMap()->GetMapPath().c_str());
    } else {
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-current_position,
            "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s(%s+%" PRIu64 ")\n", index,
            frame->GetFrameRelativePc(), frame->GetFramePc(), (frame->GetFrameMap() == nullptr) ?
            "Unknown" : frame->GetFrameMap()->GetMapPath().c_str(), frame->GetFrameFuncName().c_str(),
            frame->GetFrameFuncOffset());
    }
    return writeNumber;
}

bool DfxDumpCatcher::ExecLocalDump(const int pid, const int tid, std::string& msg)
{
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    DfxLogDebug("%s :: ExecLocalDump :: cPid(%d), cTid(%d), pid(%d), tid(%d).",
        DFXDUMPCATCHER_TAG.c_str(), currentPid, currentTid, pid, tid);

    if (((pid == currentPid) && (tid == currentTid))) {
        unw_context_t context;
        unw_getcontext(&context);

        unw_cursor_t cursor;
        unw_init_local(&cursor, &context);

        size_t index = 0;
        long current_position = 0;
        auto maps = DfxElfMaps::Create(currentPid);
        while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS)) {
            std::shared_ptr<DfxFrames> frame = std::make_shared<DfxFrames>();

            unw_word_t pc;
            if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) {
                break;
            }
            frame->SetFramePc((uint64_t)pc);

            unw_word_t sp;
            if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(sp)))) {
                break;
            }
            frame->SetFrameSp((uint64_t)sp);

            auto frameMap = frame->GetFrameMap();
            if (maps->FindMapByAddr(frame->GetFramePc(), frameMap)) {
                frame->SetFrameRelativePc(frame->GetRelativePc(maps));
            }

            char sym[SYMBOL_BUF_SIZE] = {0};
            uint64_t frameOffset = frame->GetFrameFuncOffset();
            if (unw_get_proc_name(&cursor, sym, SYMBOL_BUF_SIZE, (unw_word_t*)(&frameOffset)) == 0) {
                std::string funcName;
                std::string strSym(sym, sym + strlen(sym));
                TrimAndDupStr(strSym, funcName);
                frame->SetFrameFuncName(funcName);
                frame->SetFrameFuncOffset(frameOffset);
            }

            // skip 0 stack, as this is dump catcher. Caller don't need it.
            if (index > 0) {
                int writeNumber = WriteDumpInfo(current_position, index - 1, frame);
                current_position = current_position + writeNumber;
            }
            index++;
        }
        DfxLogDebug("%s :: ExecLocalDump :: return true.", DFXDUMPCATCHER_TAG.c_str());
        return true;
    } else {
        return false;
    }
}

void DfxDumpCatcher::FreeStackInfo()
{
    free(g_StackInfo_);
    g_StackInfo_ = nullptr;
}

bool DfxDumpCatcher::DumpCatch(const int pid, const int tid, std::string& msg)
{
    DfxLogDebug("%s :: dump_catch :: pid(%d), tid(%d), msg(%s).",
        DFXDUMPCATCHER_TAG.c_str(), pid, tid, msg.c_str());

    if (pid < 0 || tid < 0) {
        DfxLogError("%s :: dump_catch :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    g_StackInfo_ = (char *) calloc(BACK_STACK_INFO_SIZE + 1, 1);
    errno_t err = memset_s(g_StackInfo_, BACK_STACK_INFO_SIZE + 1, '\0', BACK_STACK_INFO_SIZE);
    if (err != EOK) {
        DfxLogError("%s :: dump_catch :: memset_s failed, err = %d\n", DFXDUMPCATCHER_TAG.c_str(), err);
        return false;
    }

    if (!ExecLocalDump(pid, tid, msg)) {
        do {
            int st;
            char cmd[1024] = {'\0'};

            if (tid == 0) {
                sprintf_s(cmd, sizeof(cmd), "/system/bin/processdump -p %d", pid);
            } else {
                sprintf_s(cmd, sizeof(cmd), "/system/bin/processdump -p %d -t %d", pid, tid);
            }

            FILE *fp = nullptr;
            fp = popen(cmd, "r");
            if (fp == nullptr) {
                break;
            }
            wait(&st);

            int count = fread(g_StackInfo_, 1, (BACK_STACK_INFO_SIZE - 1), fp);
            fclose(fp);

            if (count < 0) {
                break;
            }

            // if dump output is nothing, make dump_catch return false.
            if (count == 0) {
                break;
            }

            if (strncmp (g_StackInfo_, DUMP_STACK_TAG_FAILED.c_str(), DUMP_STACK_TAG_FAILED.length()) == 0) {
                msg = g_StackInfo_;
                DfxLogError("%s :: dump_catch :: got an empty stack info.", DFXDUMPCATCHER_TAG.c_str());
                break;
            }

            msg = g_StackInfo_;
            FreeStackInfo();

            return true;
        } while (false);

        FreeStackInfo();
        return false;
    } else {
        msg = g_StackInfo_;
        FreeStackInfo();

        return true;
    }
}
} // namespace HiviewDFX
} // namespace OHOS
