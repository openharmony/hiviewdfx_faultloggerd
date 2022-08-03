/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains process dump entry function. */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <securec.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "directory_ex.h"
#include "file_ex.h"

#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_dump_request.h"
#include "dump_catcher.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#include "dfx_cutil.h"
#endif

static const int DUMP_THIRD = 3;
static const int DUMP_FIVE = 5;
static const int DUMP_ARG_ONE = 1;
static const int DUMP_ARG_TWO = 2;
static const int DUMP_ARG_THREE = 3;
static const int DUMP_ARG_FOUR = 4;

static const std::string DUMP_STACK_TAG_USAGE = "usage:";
static const std::string DUMP_STACK_TAG_FAILED = "failed:";

static void PrintCommandHelp()
{
    std::cout << DUMP_STACK_TAG_USAGE << std::endl;
    std::cout << "-p pid -t tid    dump the stacktrace of the thread with given tid." << std::endl;
    std::cout << "-p pid    dump the stacktrace of all the threads with given pid." << std::endl;
    std::cout << "-T type    dump the stacktrace of the type." << std::endl;
}

static void PrintPidTidCheckFailed(int32_t pid, int32_t tid, const std::string& error)
{
    DfxLogWarn("pid:%d, tid:%d check failed", pid, tid);
    std::cout << DUMP_STACK_TAG_FAILED << std::endl;
    std::cout << error << std::endl;
    DfxLogWarn(error.c_str());
    std::cout << "The pid or tid is invalid." << std::endl;
}

static void FillErrorInfo(std::string& error, const char* format, ...)
{
    char buffer[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    int size = vsnprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, format, args);
    if (size == -1) {
        DfxLogWarn("FillErrorInfo :: vsnprintf_s fail");
    }
    va_end(args);
    std::string temp(buffer);
    error = temp;
}

// Check whether the tid is owned by pid.
static bool CheckPidTid(OHOS::HiviewDFX::ProcessDumpType type, int32_t pid, int32_t tid, std::string& error)
{
    // check pid
    if (pid <= 0) {
        FillErrorInfo(error, "pid is zero or negative.");
        return false;
    }

    // user specified a tid, we need to check tid / pid is valid or not.
    if (type == OHOS::HiviewDFX::DUMP_TYPE_THREAD) {
        if (tid > 0) {
            std::vector<std::string> files;
            std::string path = "/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid);
            OHOS::GetDirFiles(path, files);
            if (files.size() == 0) {
                FillErrorInfo(error, "Cannot find tid(%d) in process(%d).", tid, pid);
                return false;
            }
        } else {
            FillErrorInfo(error, "tid is zero or negative.");
            return false;
        }
    }

    // check pid, make sure /proc/xxx/maps is valid.
    if (pid > 0) {
        char path[NAME_LEN] = {0};
        if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/maps", pid) <= 0) {
            FillErrorInfo(error, "Fail to snprintf path.");
            return false;
        }

        FILE *fp = fopen(path, "r");
        if (fp == nullptr) {
            FillErrorInfo(error, "Fail to open maps info.");
            return false;
        }
        fclose(fp);
    } else {
        FillErrorInfo(error, "pid is zero or negative.");
        return false;
    }

    return true;
}

static bool ParseParamters(int argc, char *argv[], OHOS::HiviewDFX::ProcessDumpType &type,
    int32_t &pid, int32_t &tid)
{
    if (argc <= DUMP_ARG_ONE) {
        return false;
    }
    DfxLogDebug("argc: %d, argv1: %s", argc, argv[1]);
    switch (argc) {
        case DUMP_THIRD:
            if (!strcmp("-p", argv[DUMP_ARG_ONE])) {
                type = OHOS::HiviewDFX::DUMP_TYPE_PROCESS;
                pid = atoi(argv[DUMP_ARG_TWO]);
                return true;
            }
            break;
        case DUMP_FIVE:
            if (!strcmp("-p", argv[DUMP_ARG_ONE])) {
                type = OHOS::HiviewDFX::DUMP_TYPE_PROCESS;
                pid = atoi(argv[DUMP_ARG_TWO]);

                if (!strcmp("-t", argv[DUMP_ARG_THREE])) {
                    type = OHOS::HiviewDFX::DUMP_TYPE_THREAD;
                    tid = atoi(argv[DUMP_ARG_FOUR]);
                    return true;
                }
            } else if (!strcmp("-t", argv[DUMP_ARG_ONE])) {
                type = OHOS::HiviewDFX::DUMP_TYPE_THREAD;
                tid = atoi(argv[DUMP_ARG_TWO]);

                if (!strcmp("-p", argv[DUMP_ARG_THREE])) {
                    pid = atoi(argv[DUMP_ARG_FOUR]);
                    return true;
                }
            }
            break;
        default:
            break;
    }
    return false;
}

int main(int argc, char *argv[])
{
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_InstallLocalSignalHandler();
#endif

    OHOS::HiviewDFX::ProcessDumpType type = OHOS::HiviewDFX::DUMP_TYPE_PROCESS;
    int32_t pid = 0;
    int32_t tid = 0;

    alarm(PROCESSDUMP_TIMEOUT); // wait 30s for process dump done
    setsid();

    if (!ParseParamters(argc, argv, type, pid, tid)) {
        PrintCommandHelp();
        return 0;
    }

    std::string error;
    if (!CheckPidTid(type, pid, tid, error)) { // check pid tid is valid
        PrintPidTidCheckFailed(pid, tid, error);
        return 0;
    }

    OHOS::HiviewDFX::DumpCatcher::GetInstance().Dump(pid, tid);
    return 0;
}
