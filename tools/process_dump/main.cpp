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

/* This files contains process dump entry function. */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <directory_ex.h>
#include <file_ex.h>
#include <iostream>
#include <securec.h>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "dfx_dump_writer.h"
#include "dfx_log.h"
#include "dfx_config.h"
#include "process_dumper.h"
#include "../../interfaces/innerkits/faultloggerd_client/include/faultloggerd_client.h"


static const int SIGNAL_HANDLER = 1;
static const int DUMP_THIRD = 3;
static const int DUMP_FIVE = 5;
static const int DUMP_ARG_TWO = 2;
static const int DUMP_ARG_THREE = 3;
static const int DUMP_ARG_FOUR = 4;
static const int ALARM_TIME_S = 10;
static const std::string DUMP_STACK_TAG_FAILED = "failed:";
static const int32_t SOCKET_BUFFER_SIZE = 256;
static const char FAULTLOGGERD_SOCK_PATH[] = "/dev/unix/socket/faultloggerd.server";

static void PrintCommandHelp()
{
    std::cout << "usage:" << std::endl;
    std::cout << "-p pid -t tid    dump the stacktrace of the thread with given tid." << std::endl;
    std::cout << "-p pid    dump the stacktrace of all the threads with given pid." << std::endl;
}

static void PrintPermissionCheckFailed()
{
    std::cout << DUMP_STACK_TAG_FAILED << std::endl;
    std::cout << "Only BMS and the process owner can dump stacktrace." << std::endl;
}

static void PrintFailedConnect()
{
    std::cout << DUMP_STACK_TAG_FAILED << std::endl;
    std::cout << "faultloggerd is not available" << std::endl;
}

static void PrintPidTidCheckFailed(int32_t pid, int32_t tid)
{
    std::cout << DUMP_STACK_TAG_FAILED << std::endl;
    std::cout << "The " << tid  << " is not a thread of " << pid << std::endl;
    std::cout << "The pid or tid is invalid." << std::endl;
}

// Check whether the tid is owned by pid.
static bool CheckPidTid(OHOS::HiviewDFX::ProcessDumpType type, int32_t pid, int32_t tid)
{
    // check pid
    if ((pid == 0) || (pid < 0)) {
        DfxLogWarn("pid is zero or negative.");
        return false;
    }

    // user specified a tid, we need to check tid / pid is valid or not.
    if (type == OHOS::HiviewDFX::DUMP_TYPE_THREAD) {
        if (tid > 0) {
            std::vector<std::string> files;

            std::string path = "/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid);

            OHOS::GetDirFiles(path, files);
            if (files.size() == 0) {
                DfxLogWarn("Cannot find tid(%d) in process(%d).", tid, pid);
                return false;
            }
        } else {
            DfxLogWarn("tid is zero or negative.");
            return false;
        }
    }

    // check pid, make sure /proc/xxx/maps is valid.
    if (pid > 0) {
        std::vector<std::string> files;
        std::string path = "/proc/" + std::to_string(pid);

        OHOS::GetDirFiles(path, files);
        if (files.size() == 0) {
            DfxLogWarn("Cannot find pid(%d) in /proc.", pid);
            return false;
        }
    } else {
        DfxLogWarn("pid is zero or negative.");
        return false;
    }

    return true;
}

static bool CheckConnectStatus()
{
    int sockfd = -1;
    bool check_status = false;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        return false;
    }
    do {
        struct sockaddr_un server;
        errno_t ret = memset_s(&server, sizeof(server), 0, sizeof(server));
        if (ret != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)ret);
            break;
        }
        server.sun_family = AF_LOCAL;
        if (strncpy_s(server.sun_path, sizeof(server.sun_path),
            FAULTLOGGERD_SOCK_PATH, strlen(FAULTLOGGERD_SOCK_PATH)) != 0) {
            break;
        }

        int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
        int connect_status = connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len);
        if (connect_status == 0) {
            check_status = true;
        }
    } while (false);
    close(sockfd);
    return check_status;
}

static bool ParseParamters(int argc, char *argv[], bool &isSignalHdlr, OHOS::HiviewDFX::ProcessDumpType &type,
    int32_t &pid, int32_t &tid)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (argc) {
        case SIGNAL_HANDLER:
            if (!strcmp("-signalhandler", argv[0])) {
                isSignalHdlr = true;
                return true;
            }
            break;
        case DUMP_THIRD:
            if (!strcmp("-p", argv[1])) {
                type = OHOS::HiviewDFX::DUMP_TYPE_PROCESS;
                pid = atoi(argv[DUMP_ARG_TWO]);
                return true;
            }
            break;
        case DUMP_FIVE:
            if (!strcmp("-p", argv[1])) {
                type = OHOS::HiviewDFX::DUMP_TYPE_PROCESS;
                pid = atoi(argv[DUMP_ARG_TWO]);

                if (!strcmp("-t", argv[DUMP_ARG_THREE])) {
                    type = OHOS::HiviewDFX::DUMP_TYPE_THREAD;
                    tid = atoi(argv[DUMP_ARG_FOUR]);
                    return true;
                }
            } else if (!strcmp("-t", argv[1])) {
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
    DfxLogDebug("Exit %s.", __func__);
    return false;
}

int main(int argc, char *argv[])
{
    DfxLogDebug("Enter %s.", __func__);
    bool isSignalHdlr = false;
    OHOS::HiviewDFX::ProcessDumpType type = OHOS::HiviewDFX::DUMP_TYPE_PROCESS;
    int32_t pid = 0;
    int32_t tid = 0;

    alarm(ALARM_TIME_S); // wait 10s for process dump done

    if (!ParseParamters(argc, argv, isSignalHdlr, type, pid, tid)) {
        PrintCommandHelp();
        return 0;
    }

    if (!isSignalHdlr) { // We need do permission check when "false == isSignalHdlr" dump.
        if (!CheckConnectStatus()) {
            PrintFailedConnect();
            return 0;
        }
        if (!CheckPidTid(type, pid, tid)) { // check pid tid is valid
            PrintPidTidCheckFailed(pid, tid);
            return 0;
        }
        if (!RequestCheckPermission(pid)) { // check permission
            PrintPermissionCheckFailed();
            return 0;
        }
    }

    OHOS::HiviewDFX::DfxConfig::GetInstance().readConfig();
    OHOS::HiviewDFX::ProcessDumper::GetInstance().Dump(isSignalHdlr, type, pid, tid);
    DfxLogDebug("End");

    DfxLogInfo("faultlog_native_crash");
    return 0;
}
