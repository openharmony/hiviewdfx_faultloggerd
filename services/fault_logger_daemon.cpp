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

#include "fault_logger_daemon.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "directory_ex.h"
#include "file_ex.h"
#include <securec.h>

#include "dfx_log.h"
#include "dfx_define.h"
#include "fault_logger_config.h"
#include "fault_logger_secure.h"
#include "faultloggerd_socket.h"
#include "fault_logger_pipe.h"

using namespace std::chrono;

namespace OHOS {
namespace HiviewDFX {
using FaultLoggerdRequest = struct FaultLoggerdRequest;
std::shared_ptr<FaultLoggerConfig> faultLoggerConfig_;
std::shared_ptr<FaultLoggerSecure> faultLoggerSecure_;
std::shared_ptr<FaultLoggerPipeMap> faultLoggerPipeMap_;
FaultLoggerDaemon* FaultLoggerDaemon::faultLoggerDaemon_ = nullptr;

namespace {
constexpr int32_t MAX_CONNECTION = 30;
constexpr int32_t REQUEST_BUF_SIZE = 1024;

const int32_t FAULTLOG_FILE_PROP = 0640;

static const std::string FAULTLOGGERD_TAG = "FaultLoggerd";

static const std::string DAEMON_RESP = "RESP:COMPLETE";

const int MINUS_ONE_THOUSAND = -1000;
static const int DAEMON_REMOVE_FILE_TIME_S = 60;

static std::string GetRequestTypeName(int32_t type)
{
    switch (type) {
        case (int32_t)FaultLoggerType::CPP_CRASH:
            return "cppcrash";
        case (int32_t)FaultLoggerType::CPP_STACKTRACE: // change the name to nativestack ?
            return "stacktrace";
        case (int32_t)FaultLoggerType::JS_STACKTRACE:
            return "jsstack";
        case (int32_t)FaultLoggerType::JS_HEAP_SNAPSHOT:
            return "jsheap";
        default:
            return "unsupported";
    }
}
}

FaultLoggerDaemon::FaultLoggerDaemon()
{
    faultLoggerDaemon_ = this;
}

int32_t FaultLoggerDaemon::StartServer()
{
    int socketFd = -1;
    if (!StartListen(socketFd, FAULTLOGGERD_SOCK_PATH, strlen(FAULTLOGGERD_SOCK_PATH), MAX_CONNECTION)) {
        DfxLogError("%s :: Failed to start listen", FAULTLOGGERD_TAG.c_str());
        return -1;
    }

    if (!InitEnvironment()) {
        DfxLogError("%s :: Failed to init environment", FAULTLOGGERD_TAG.c_str());
        close(socketFd);
        return -1;
    }

    DfxLogInfo("%s :: %s: start loop accept.", FAULTLOGGERD_TAG.c_str(), __func__);
    LoopAcceptRequestAndFork(socketFd);

    close(socketFd);
    return 0;
}

bool FaultLoggerDaemon::InitEnvironment()
{
    DfxLogInfo("%s :: %s Enter.", OHOS::HiviewDFX::FAULTLOGGERD_TAG.c_str(), __func__);

    faultLoggerConfig_ = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    faultLoggerSecure_ = std::make_shared<FaultLoggerSecure>();
    faultLoggerPipeMap_ = std::make_shared<FaultLoggerPipeMap>();

    if (!OHOS::ForceCreateDirectory(faultLoggerConfig_->GetLogFilePath())) {
        DfxLogError("%s :: Failed to ForceCreateDirectory GetLogFilePath", FAULTLOGGERD_TAG.c_str());
        return false;
    }

    if (!OHOS::ForceCreateDirectory(faultLoggerConfig_->GetDebugLogFilePath())) {
        DfxLogError("%s :: Failed to ForceCreateDirectory GetDebugLogFilePath", FAULTLOGGERD_TAG.c_str());
        return false;
    }

    if (chmod(FAULTLOGGERD_SOCK_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH) < 0) {
        DfxLogError("%s :: Failed to chmod, %d", FAULTLOGGERD_TAG.c_str(), errno);
    }

    DfxLogInfo("%s :: %s success finished.", OHOS::HiviewDFX::FAULTLOGGERD_TAG.c_str(), __func__);
    return true;
}

void FaultLoggerDaemon::HandleDefaultClientReqeust(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    RemoveTempFileIfNeed();

    int fd = CreateFileForRequest(request->type, request->pid, request->time, false);
    if (fd < 0) {
        DfxLogError("%s :: Failed to create log file", FAULTLOGGERD_TAG.c_str());
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandleLogFileDesClientReqeust(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    int fd = CreateFileForRequest(request->type, request->pid, request->time, true);
    if (fd < 0) {
        DfxLogError("%s :: Failed to create log file", FAULTLOGGERD_TAG.c_str());
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandlePipeFdClientReqeust(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    DfxLogInfo("%s :: pid(%d), pipeType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->pid, request->pipeType);
    int fd = -1;

    if (request->pipeType == (int32_t)FaultLoggerPipeType::PIPE_FD_DELETE) {
        faultLoggerPipeMap_->Del(request->pid);
        return;
    }
    FaultLoggerPipe2* faultLoggerPipe = faultLoggerPipeMap_->Get(request->pid);
    switch (request->pipeType) {
        case (int32_t)FaultLoggerPipeType::PIPE_FD_READ_BUF:
            fd = faultLoggerPipe->faultLoggerPipeBuf_->GetReadFd();
            break;
        case (int32_t)FaultLoggerPipeType::PIPE_FD_WRITE_BUF:
            fd = faultLoggerPipe->faultLoggerPipeBuf_->GetWriteFd();
            break;
        case (int32_t)FaultLoggerPipeType::PIPE_FD_READ_RES:
            fd = faultLoggerPipe->faultLoggerPipeRes_->GetReadFd();
            break;
        case (int32_t)FaultLoggerPipeType::PIPE_FD_WRITE_RES:
            fd = faultLoggerPipe->faultLoggerPipeRes_->GetWriteFd();
            break;
        default:
            DfxLogError("%s :: unknown pipeType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->pipeType);
            break;
    }

    if (fd < 0) {
        DfxLogError("%s :: Failed to get pipe fd", FAULTLOGGERD_TAG.c_str());
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);
}

void FaultLoggerDaemon::HandlePrintTHilogClientReqeust(int32_t const connectionFd, FaultLoggerdRequest * request)
{
    char buf[LOG_BUF_LEN] = {0};

    if (write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length()) != (ssize_t)DAEMON_RESP.length()) {
        DfxLogError("%s :: Failed to write DAEMON_RESP.", FAULTLOGGERD_TAG.c_str());
    }

    int nread = read(connectionFd, buf, sizeof(buf) - 1);
    if (nread < 0) {
        DfxLogError("%s :: Failed to read message", FAULTLOGGERD_TAG.c_str());
    } else if (nread == 0) {
        DfxLogError("%s :: HandlePrintTHilogClientReqeust :: Read null from request socket", FAULTLOGGERD_TAG.c_str());
    } else {
        DfxLogError("%s", buf);
    }
}

FaultLoggerCheckPermissionResp FaultLoggerDaemon::SecurityCheck(int32_t connectionFd, FaultLoggerdRequest * request)
{
    FaultLoggerCheckPermissionResp resCheckPermission = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;

    struct ucred rcred;
    do {
        int optval = 1;
        if (setsockopt(connectionFd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) == -1) {
            DfxLogError("%s :: setsockopt SO_PASSCRED error.", FAULTLOGGERD_TAG.c_str());
            break;
        }
        
        if (write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length()) != (ssize_t)DAEMON_RESP.length()) {
            DfxLogError("%s :: Failed to write DAEMON_RESP.", FAULTLOGGERD_TAG.c_str());
        }

        if (!RecvMsgCredFromSocket(connectionFd, &rcred)) {
            DfxLogError("%s :: Recv msg ucred error.", FAULTLOGGERD_TAG.c_str());
            break;
        }

        request->uid = rcred.uid;
        request->callerPid = (int32_t)rcred.pid;
        bool res = faultLoggerSecure_->CheckCallerUID((int)request->uid, request->pid);
        if (res) {
            resCheckPermission = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS;
        }
    } while (false);

    return resCheckPermission;
}

void FaultLoggerDaemon::HandlePermissionReqeust(int32_t connectionFd, FaultLoggerdRequest * request)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS == resSecurityCheck) {
        send(connectionFd, "1", strlen("1"), 0);
    }
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT == resSecurityCheck) {
        send(connectionFd, "2", strlen("2"), 0);
    }
}

void FaultLoggerDaemon::HandleSdkDumpReqeust(int32_t connectionFd, FaultLoggerdRequest * request)
{
    FaultLoggerSdkDumpResp resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_REJECT;
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);

    /*
    *           all     threads my user, local pid             my user, remote pid     other user's process
    * 3rd       Y       Y(in signal_handler local)     Y(in signal_handler loacl)      N
    * system    Y       Y(in signal_handler local)     Y(in signal_handler loacl)      Y(in signal_handler remote)
    * root      Y       Y(in signal_handler local)     Y(in signal_handler loacl)      Y(in signal_handler remote)
    */

    /*
    * 1. pid != 0 && tid != 0:    means we want dump a thread, so we send signal to a thread.
        Main thread stack is tid's stack, we need ignore other thread info.
    * 2. pid != 0 && tid == 0:    means we want dump a process, so we send signal to process.
        Main thead stack is pid's stack, we need other tread info.
    */

    /*
     * in signal_handler we need to check caller pid and tid(which is send to signal handler by SYS_rt_sig.).
     * 1. caller pid == signal pid, means we do back trace in ourself process, means local backtrace.
     *      |- we do all tid back trace in signal handler's local unwind.
     * 2. pid != signal pid, means we do remote back trace.
     */

    /*
     * in local back trace, all unwind stack will save to signal_handler global var.(mutex lock in signal handler.)
     * in remote back trace, all unwind stack will save to file, and read in dump_catcher, then return.
     */

    do {
        if ((request->pid <= 0) || (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT == resSecurityCheck)) {
            DfxLogError("%s :: HandleSdkDumpReqeust :: pid(%d) or resSecurityCheck(%d) fail.\n", \
                        FAULTLOGGERD_TAG.c_str(), request->pid, (int)resSecurityCheck);
            break;
        }

        if (faultLoggerPipeMap_->Find(request->pid)) {
            DfxLogError("%s :: pid(%d) is dumping, break.\n", FAULTLOGGERD_TAG.c_str(), request->pid);
            break;
        }
        std::shared_ptr<FaultLoggerPipe2> ptr = std::make_shared<FaultLoggerPipe2>();
        faultLoggerPipeMap_->Set(request->pid, ptr);

        int sig = SIGDUMP;
        // defined in out/hi3516dv300/obj/third_party/musl/intermidiates/linux/musl_src_ported/include/signal.h
        siginfo_t si = {
            .si_signo = SIGDUMP,
            .si_errno = 0,
            .si_code = MINUS_ONE_THOUSAND,
            .si_pid = request->callerPid,
            .si_uid = static_cast<uid_t>(request->callerTid)
        };

        // means we need dump all the threads in a process.
        if (request->tid == 0) {
            if (syscall(SYS_rt_sigqueueinfo, request->pid, sig, &si) != 0) {
                DfxLogError("Failed to SYS_rt_sigqueueinfo signal(%d), errno(%d).", si.si_signo, errno);
                break;
            }
        } else {
            // means we need dump a specified thread
            if (syscall(SYS_rt_tgsigqueueinfo, request->pid, request->tid, sig, &si) != 0) {
                DfxLogError("Failed to SYS_rt_tgsigqueueinfo signal(%d), errno(%d).", si.si_signo, errno);
                break;
            }
        }
        resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_PASS;
    } while (false);

    if (FaultLoggerSdkDumpResp::SDK_DUMP_PASS == resSdkDump) {
        send(connectionFd, "1", strlen("1"), 0);
    }

    if (FaultLoggerSdkDumpResp::SDK_DUMP_REJECT == resSdkDump) {
        send(connectionFd, "2", strlen("2"), 0);
    }
}

void FaultLoggerDaemon::HandleRequesting(int32_t connectionFd)
{
    faultLoggerDaemon_->HandleRequest(connectionFd);
}

void FaultLoggerDaemon::HandleRequest(int32_t connectionFd)
{
    ssize_t nread = -1;
    char buf[REQUEST_BUF_SIZE] = {0};

    do {
        nread = read(connectionFd, buf, sizeof(buf));
        if (nread < 0) {
            DfxLogError("%s :: Failed to read message", FAULTLOGGERD_TAG.c_str());
            break;
        } else if (nread == 0) {
            DfxLogError("%s :: HandleRequest :: Read null from request socket", FAULTLOGGERD_TAG.c_str());
            break;
        } else if (nread != static_cast<long>(sizeof(FaultLoggerdRequest))) {
            DfxLogError("%s :: Unmatched request length", FAULTLOGGERD_TAG.c_str());
            break;
        }

        auto request = reinterpret_cast<FaultLoggerdRequest *>(buf);
        DfxLogInfo("%s :: clientType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->clientType);
        switch (request->clientType) {
            case (int32_t)FaultLoggerClientType::DEFAULT_CLIENT:
                HandleDefaultClientReqeust(connectionFd, request);
                break;
            case (int32_t)FaultLoggerClientType::LOG_FILE_DES_CLIENT:
                HandleLogFileDesClientReqeust(connectionFd, request);
                break;
            case (int32_t)FaultLoggerClientType::PRINT_T_HILOG_CLIENT:
                HandlePrintTHilogClientReqeust(connectionFd, request);
                break;
            case (int32_t)FaultLoggerClientType::PERMISSION_CLIENT:
                HandlePermissionReqeust(connectionFd, request);
                break;
            case (int32_t)FaultLoggerClientType::SDK_DUMP_CLIENT:
                HandleSdkDumpReqeust(connectionFd, request);
                break;
            case (int32_t)FaultLoggerClientType::PIPE_FD_CLIENT:
                HandlePipeFdClientReqeust(connectionFd, request);
                break;
            default:
                DfxLogError("%s :: unknown clientType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->clientType);
                break;
        }
    } while (false);

    close(connectionFd);
}

int32_t FaultLoggerDaemon::CreateFileForRequest(int32_t type, int32_t pid, uint64_t time, bool debugFlag) const
{
    std::string typeStr = GetRequestTypeName(type);
    if (typeStr == "unsupported") {
        DfxLogError("Unsupported request type(%d)", type);
        return -1;
    }

    std::string filePath = "";
    if (debugFlag == false) {
        filePath = faultLoggerConfig_->GetLogFilePath();
    } else {
        filePath = faultLoggerConfig_->GetDebugLogFilePath();
    }

    if (time == 0) {
        time = (uint64_t)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    std::stringstream crashTime;
    crashTime << "-" << time;
    std::string path = filePath + "/" + typeStr + "-" + std::to_string(pid) + crashTime.str();

    DfxLogInfo("%s :: file path(%s).\n", FAULTLOGGERD_TAG.c_str(), path.c_str());
    int32_t fd = open(path.c_str(), O_RDWR | O_CREAT, FAULTLOG_FILE_PROP);
    if (fd != -1) {
        if (!ChangeModeFile(path, FAULTLOG_FILE_PROP)) {
            DfxLogError("%s :: Failed to ChangeMode CreateFileForRequest", FAULTLOGGERD_TAG.c_str());
        }
    }
    return fd;
}

void FaultLoggerDaemon::RemoveTempFileIfNeed()
{
    int maxFileCount = 50;
    int currentLogCounts = 0;

    std::vector<std::string> files;
    OHOS::GetDirFiles(faultLoggerConfig_->GetLogFilePath(), files);
    currentLogCounts = (int)files.size();

    maxFileCount = faultLoggerConfig_->GetLogFileMaxNumber();
    if (currentLogCounts < maxFileCount) {
        return;
    }

    std::sort(files.begin(), files.end(),
        [](const std::string& lhs, const std::string& rhs) -> int
    {
        auto lhsSplitPos = lhs.find_last_of("-");
        auto rhsSplitPos = rhs.find_last_of("-");
        if (lhsSplitPos == std::string::npos || rhsSplitPos == std::string::npos) {
            return lhs.compare(rhs) > 0;
        }

        return lhs.substr(lhsSplitPos).compare(rhs.substr(rhsSplitPos)) > 0;
    });

    time_t currentTime = static_cast<time_t>(time(nullptr));
    if (currentTime <= 0) {
        DfxLogError("%s :: currentTime is less than zero CreateFileForRequest", FAULTLOGGERD_TAG.c_str());
    }

    int startIndex = maxFileCount / 2;
    for (unsigned int index = (unsigned int)startIndex; index < files.size(); index++) {
        struct stat st;
        int err = stat(files[index].c_str(), &st);
        if (err != 0) {
            DfxLogError("%s :: Get log stat failed.", FAULTLOGGERD_TAG.c_str());
        } else {
            if ((currentTime - st.st_mtime) <= DAEMON_REMOVE_FILE_TIME_S) {
                continue;
            }
        }

        OHOS::RemoveFile(files[index]);
        DfxLogDebug("%s :: Now we rm file(%s) as max log number exceeded.", \
                    FAULTLOGGERD_TAG.c_str(), files[index].c_str());
    }
}

void FaultLoggerDaemon::LoopAcceptRequestAndFork(int socketFd)
{
    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));
    int connectionFd = -1;
    signal(SIGCHLD, SIG_IGN);

    while (true) {
        connectionFd = accept(socketFd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize);
        if (connectionFd < 0) {
            DfxLogError("%s :: Failed to accept connection", FAULTLOGGERD_TAG.c_str());
            continue;
        }
        DfxLogInfo("%s :: %s: accept: %d.", FAULTLOGGERD_TAG.c_str(), __func__, connectionFd);

        std::thread th(FaultLoggerDaemon::HandleRequesting, connectionFd);
        th.detach();
    }
}
} // namespace HiviewDFX
} // namespace OHOS
