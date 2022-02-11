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
#include <climits>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
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
#include "../signal_handler/include/dfx_signal_handler.h"
#include "dfx_define.h"
#include "dfx_dump_writer.h"
#include "dfx_log.h"
#include "dfx_process.h"
#include "dfx_thread.h"
#include "dfx_util.h"

static const int SYMBOL_BUF_SIZE = 4096;
static const int NUMBER_ONE = 1;
static const int NUMBER_TWO = 2;
static const int SLEEP_TIME_TEN_S = 10;
static const int SLEEP_TIME_TWENTY_S = 20;

static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";
static const std::string DUMP_STACK_TAG_FAILED = "failed:";

const std::string LOG_FILE_PATH = "/data/log/faultlog/temp";

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

bool DfxDumpCatcher::ExecLocalDump(const int pid, const int tid, const int skipFramNum)
{
    DfxLogDebug("%s :: ExecLocalDump :: pid(%d), tid(%d), skipFramNum(%d).",
        DFXDUMPCATCHER_TAG.c_str(), pid, tid, skipFramNum);
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    size_t index = 0;
    auto maps = DfxElfMaps::Create(pid);
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
        if (index >= skipFramNum) {
            int writeNumber = WriteDumpInfo(g_CurrentPosition, index - skipFramNum, frame);
            g_CurrentPosition = g_CurrentPosition + writeNumber;
        }
        index++;
    }

    DfxLogDebug("%s :: ExecLocalDump :: return true.", DFXDUMPCATCHER_TAG.c_str());
    return true;
}

void DfxDumpCatcher::FreeStackInfo()
{
    free(g_StackInfo_);
    g_StackInfo_ = nullptr;
    g_CurrentPosition = 0;
}

bool DfxDumpCatcher::DoDumpLocalPidTid(const int pid, const int tid)
{
    DfxLogDebug("%s :: DoDumpLocalPidTid :: pid(%d), tid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid, tid);
    if (pid <= 0 || tid <= 0) {
        DfxLogError("%s :: DoDumpLocalPidTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    do {
        if (RequestSdkDump(pid, tid) == true) {
            int time = 0;
            long long msgLength;
            do {
                msgLength = g_CurrentPosition;
                DfxLogDebug("%s :: DoDumpLocalPidTid :: msgLength(%lld), time(%d).",
                    DFXDUMPCATCHER_TAG.c_str(), msgLength, time);

                sleep(1);
                time++;

                DfxLogDebug("%s :: DoDumpLocalPidTid :: g_CurrentPosition(%lld), time(%d).",
                    DFXDUMPCATCHER_TAG.c_str(), g_CurrentPosition, time);
            } while (time < SLEEP_TIME_TWENTY_S && msgLength != g_CurrentPosition);

            DfxLogDebug("%s :: DoDumpLocalPidTid :: return true.", DFXDUMPCATCHER_TAG.c_str());
            return true;
        } else {
            break;
        }
    } while (false);

    DfxLogError("%s :: DoDumpLocalPidTid :: return false.", DFXDUMPCATCHER_TAG.c_str());
    return false;
}

bool DfxDumpCatcher::DoDumpLocalPid(const int pid)
{
    DfxLogDebug("%s :: DoDumpLocalPid :: pid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid);
    if (pid <= 0) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/task", pid) <= 0) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as snprintf_s failed.", DFXDUMPCATCHER_TAG.c_str());
        return FALSE;
    }

    char realPath[PATH_MAX] = {'\0'};
    if (realpath(path, realPath) == NULL) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as realpath failed.", DFXDUMPCATCHER_TAG.c_str());
        return FALSE;
    }

    DIR *dir = opendir(realPath);
    if (dir == NULL) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as opendir failed.", DFXDUMPCATCHER_TAG.c_str());
        return FALSE;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0) {
            continue;
        }

        if (strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        pid_t tid = atoi(ent->d_name);
        if (tid == 0) {
            continue;
        }

        int writeNumber = 0;
        char* current_pos = g_StackInfo_ + g_CurrentPosition;
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-g_CurrentPosition, "Tid: %d\n", tid);
        if (writeNumber >= 0) {
            g_CurrentPosition = g_CurrentPosition + writeNumber;
        }

        int currentTid = syscall(SYS_gettid);
        if (tid == currentTid) {
            ExecLocalDump(pid, tid, NUMBER_TWO);
        } else {
            DoDumpLocalPidTid(pid, tid);
        }
    }
    DfxLogDebug("%s :: DoDumpLocalPid :: return true.", DFXDUMPCATCHER_TAG.c_str());
    return true;
}

bool DfxDumpCatcher::DoDumpRemote(const int pid, const int tid)
{
    DfxLogDebug("%s :: DoDumpRemote :: pid(%d), tid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid, tid);
    if (pid <= 0 || tid < 0) {
        DfxLogError("%s :: DoDumpRemote :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    if (RequestSdkDump(pid, tid) == true) {
        int time = 0;
        // Get stack trace file
        long long stackFileLength = 0;
        std::string stackTraceFilePatten = LOG_FILE_PATH + "/stacktrace-" + std::to_string(pid);
        std::string stackTraceFileName = "";
        do {
            sleep(1);
            std::vector<std::string> files;
            OHOS::GetDirFiles(LOG_FILE_PATH, files);
            int i = 0;
            while (i < files.size() && files[i].find(stackTraceFilePatten) == std::string::npos) {
                i++;
            }
            if (i < files.size()) {
                stackTraceFileName = files[i];
            }
            time++;
        } while (stackTraceFileName == "" && time < SLEEP_TIME_TWENTY_S);
        DfxLogDebug("%s :: DoDumpRemote :: Got stack trace file %s.",
            DFXDUMPCATCHER_TAG.c_str(), stackTraceFileName.c_str());

        // Get stack trace file length
        time = 0;
        do {
            double filesize = -1;
            struct stat fileInfo;

            sleep(1);
            time++;

            if (stat(stackTraceFileName.c_str(), &fileInfo) < 0) {
                continue;
            } else if (stackFileLength == fileInfo.st_size) {
                break;
            } else {
                stackFileLength = fileInfo.st_size;
            }
        } while (time < SLEEP_TIME_TEN_S);
        DfxLogDebug("%s :: DoDumpRemote :: Got file length %lld.", DFXDUMPCATCHER_TAG.c_str(), stackFileLength);

        // Read stack trace file content.
        FILE *fp = nullptr;
        fp = fopen(stackTraceFileName.c_str(), "r");
        if (fp == nullptr) {
            DfxLogError("%s :: DoDumpRemote :: open %s NG.", DFXDUMPCATCHER_TAG.c_str(), stackTraceFileName.c_str());
            return false;
        }
        fread(g_StackInfo_, 1, BACK_STACK_INFO_SIZE, fp);
        DfxLogDebug("%s :: DoDumpRemote :: Readed stack trace file content.", DFXDUMPCATCHER_TAG.c_str());
        fclose(fp);
        fp = nullptr;
        OHOS::RemoveFile(stackTraceFileName);

        // As tid is specified, we need del other thread info, just keep tid back trace stack info.
        if (tid != 0) {
            char *cutPosition = strstr(g_StackInfo_, "Other thread info:");
            if (cutPosition != NULL) {
                *cutPosition = '\0';
            }
        }
        DfxLogDebug("%s :: DoDumpRemote :: return true.", DFXDUMPCATCHER_TAG.c_str());
        return true;
    }
    DfxLogError("%s :: DoDumpRemote :: return false.", DFXDUMPCATCHER_TAG.c_str());
    return false;
}

bool DfxDumpCatcher::DumpCatch(const int pid, const int tid, std::string& msg)
{
    bool ret = false;
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    DfxLogDebug("%s :: dump_catch :: cPid(%d), cTid(%d), pid(%d), tid(%d).",
        DFXDUMPCATCHER_TAG.c_str(), currentPid, currentTid, pid, tid);

    if (pid <= 0 || tid < 0) {
        DfxLogError("%s :: dump_catch :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    g_StackInfo_ = (char *) calloc(BACK_STACK_INFO_SIZE + 1, 1);
    errno_t err = memset_s(g_StackInfo_, BACK_STACK_INFO_SIZE + 1, '\0', BACK_STACK_INFO_SIZE);
    if (err != EOK) {
        DfxLogError("%s :: dump_catch :: memset_s failed, err = %d\n", DFXDUMPCATCHER_TAG.c_str(), err);
        return false;
    }
    g_CurrentPosition = 0;

    if (pid == currentPid) {
        if (tid == currentTid) {
            ret = ExecLocalDump(pid, tid, NUMBER_ONE);
        } else if (tid == 0) {
            ret = DoDumpLocalPid(pid);
        } else {
            ret = DoDumpLocalPidTid(pid, tid);
        }
    } else {
        ret = DoDumpRemote(pid, tid);
    }

    if (ret == true) {
        msg = g_StackInfo_;
    }

    FreeStackInfo();
    DfxLogDebug("%s :: dump_catch :: ret(%d), msg(%s).", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
