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
#include <csignal>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <securec.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "dfx_define.h"
#include "dfx_log.h"
#include "directory_ex.h"
#include "fault_logger_config.h"
#include "fault_logger_pipe.h"
#include "fault_logger_secure.h"
#include "faultloggerd_socket.h"
#include "file_ex.h"

namespace OHOS {
namespace HiviewDFX {
using FaultLoggerdRequest = struct FaultLoggerdRequest;
std::shared_ptr<FaultLoggerConfig> faultLoggerConfig_;
std::shared_ptr<FaultLoggerSecure> faultLoggerSecure_;
std::shared_ptr<FaultLoggerPipeMap> faultLoggerPipeMap_;

namespace {
constexpr int32_t MAX_CONNECTION = 30;
constexpr int32_t REQUEST_BUF_SIZE = 1024;
constexpr int32_t MAX_EPOLL_EVENT = 1024;
const int32_t FAULTLOG_FILE_PROP = 0640;

static const std::string FAULTLOGGERD_TAG = "FaultLoggerd";

static const std::string DAEMON_RESP = "RESP:COMPLETE";

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
}

int32_t FaultLoggerDaemon::StartServer()
{
    int socketFd = -1;
    if (!StartListen(socketFd, SERVER_SOCKET_NAME, MAX_CONNECTION)) {
        DfxLogError("%s :: Failed to start listen", FAULTLOGGERD_TAG.c_str());
        return -1;
    }

    if (!InitEnvironment()) {
        DfxLogError("%s :: Failed to init environment", FAULTLOGGERD_TAG.c_str());
        close(socketFd);
        return -1;
    }

    int epollFd = epoll_create(MAX_EPOLL_EVENT);
    if (epollFd < 0) {
        DfxLogError("%s :: %s: epoll_create failed.", FAULTLOGGERD_TAG.c_str(), __func__);
        close(socketFd);
        return -1;
    }
    signal(SIGCHLD, SIG_IGN);
    AddEvent(epollFd, socketFd, EPOLLIN);

    epoll_event events[MAX_CONNECTION];
    DfxLogDebug("%s :: %s: start epoll wait.", FAULTLOGGERD_TAG.c_str(), __func__);
    while (true) {
        size_t epollNum = epoll_wait(epollFd, events, MAX_CONNECTION, -1);
        for (size_t i = 0; i < epollNum; i++) {
            if ((events[i].events & EPOLLIN) != EPOLLIN) {
                continue;
            }
            int fd = events[i].data.fd;

            if (fd == socketFd) {
                HandleAccept(epollFd, socketFd);
            } else {
                HandleRequest(epollFd, events[i].data.fd);
            }
        }
    }

    if (socketFd > 0) {
        close(socketFd);
    }
    if (epollFd > 0) {
        close(epollFd);
    }
    return 0;
}

void FaultLoggerDaemon::HandleAccept(int32_t epollFd, int32_t socketFd)
{
    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));

    int connectionFd = accept(socketFd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize);
    if (connectionFd < 0) {
        DfxLogError("%s :: Failed to accept connection", FAULTLOGGERD_TAG.c_str());
        return;
    }
    DfxLogDebug("%s :: %s: accept: %d.", FAULTLOGGERD_TAG.c_str(), __func__, connectionFd);
    AddEvent(epollFd, connectionFd, EPOLLIN);
}

void FaultLoggerDaemon::HandleRequest(int32_t epollFd, int32_t connectionFd)
{
    char buf[REQUEST_BUF_SIZE] = {0};

    do {
        ssize_t nread = read(connectionFd, buf, sizeof(buf));
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
        DfxLogDebug("%s :: clientType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->clientType);
        switch (request->clientType) {
            case static_cast<int32_t>(FaultLoggerClientType::DEFAULT_CLIENT):
                HandleDefaultClientRequest(connectionFd, request);
                break;
            case static_cast<int32_t>(FaultLoggerClientType::LOG_FILE_DES_CLIENT):
                HandleLogFileDesClientRequest(connectionFd, request);
                break;
            case static_cast<int32_t>(FaultLoggerClientType::PRINT_T_HILOG_CLIENT):
                HandlePrintTHilogClientRequest(connectionFd, request);
                break;
            case static_cast<int32_t>(FaultLoggerClientType::PERMISSION_CLIENT):
                HandlePermissionRequest(connectionFd, request);
                break;
            case static_cast<int32_t>(FaultLoggerClientType::SDK_DUMP_CLIENT):
                HandleSdkDumpRequest(connectionFd, request);
                break;
            case static_cast<int32_t>(FaultLoggerClientType::PIPE_FD_CLIENT):
                HandlePipeFdClientRequest(connectionFd, request);
                break;
            default:
                DfxLogError("%s :: unknown clientType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->clientType);
                break;
        }
    } while (false);

    close(connectionFd);
    DelEvent(epollFd, connectionFd, EPOLLIN);
}

bool FaultLoggerDaemon::InitEnvironment()
{
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
    return true;
}

void FaultLoggerDaemon::HandleDefaultClientRequest(int32_t connectionFd, const FaultLoggerdRequest * request)
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

void FaultLoggerDaemon::HandleLogFileDesClientRequest(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    int fd = CreateFileForRequest(request->type, request->pid, request->time, true);
    if (fd < 0) {
        DfxLogError("%s :: Failed to create log file", FAULTLOGGERD_TAG.c_str());
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandlePipeFdClientRequest(int32_t connectionFd, FaultLoggerdRequest * request)
{
    DfxLogDebug("%s :: pid(%d), pipeType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->pid, request->pipeType);
    int fd = -1;

    FaultLoggerPipe2* faultLoggerPipe = faultLoggerPipeMap_->Get(request->pid);
    if (faultLoggerPipe == nullptr) {
        DfxLogError("%s :: cannot find pipe fd for pid(%d).\n", FAULTLOGGERD_TAG.c_str(), request->pid);
        return;
    }

    switch (request->pipeType) {
        case (int32_t)FaultLoggerPipeType::PIPE_FD_READ_BUF: {
            FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
            if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS != resSecurityCheck) {
                return;
            }
            fd = faultLoggerPipe->faultLoggerPipeBuf_->GetReadFd();
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_WRITE_BUF: {
            fd = faultLoggerPipe->faultLoggerPipeBuf_->GetWriteFd();
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_READ_RES: {
            FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
            if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS != resSecurityCheck) {
                return;
            }
            fd = faultLoggerPipe->faultLoggerPipeRes_->GetReadFd();
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_WRITE_RES: {
            fd = faultLoggerPipe->faultLoggerPipeRes_->GetWriteFd();
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_DELETE: {
            faultLoggerPipeMap_->Del(request->pid);
            return;
        }
        default:
            DfxLogError("%s :: unknown pipeType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->pipeType);
            return;
    }

    if (fd < 0) {
        DfxLogError("%s :: Failed to get pipe fd", FAULTLOGGERD_TAG.c_str());
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);
}

void FaultLoggerDaemon::HandlePrintTHilogClientRequest(int32_t const connectionFd, FaultLoggerdRequest * request)
{
    char buf[LOG_BUF_LEN] = {0};

    if (write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length()) != static_cast<ssize_t>(DAEMON_RESP.length())) {
        DfxLogError("%s :: Failed to write DAEMON_RESP.", FAULTLOGGERD_TAG.c_str());
    }

    int nread = read(connectionFd, buf, sizeof(buf) - 1);
    if (nread < 0) {
        DfxLogError("%s :: Failed to read message", FAULTLOGGERD_TAG.c_str());
    } else if (nread == 0) {
        DfxLogError("%s :: HandlePrintTHilogClientRequest :: Read null from request socket", FAULTLOGGERD_TAG.c_str());
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

        if (write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length()) !=
            static_cast<ssize_t>(DAEMON_RESP.length())) {
            DfxLogError("%s :: Failed to write DAEMON_RESP.", FAULTLOGGERD_TAG.c_str());
        }

        if (!RecvMsgCredFromSocket(connectionFd, &rcred)) {
            DfxLogError("%s :: Recv msg ucred error.", FAULTLOGGERD_TAG.c_str());
            break;
        }

        request->uid = rcred.uid;
        request->callerPid = static_cast<int32_t>(rcred.pid);
        bool res = faultLoggerSecure_->CheckCallerUID(static_cast<int>(request->uid), request->pid);
        if (res) {
            resCheckPermission = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS;
        }
    } while (false);

    return resCheckPermission;
}

void FaultLoggerDaemon::HandlePermissionRequest(int32_t connectionFd, FaultLoggerdRequest * request)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS == resSecurityCheck) {
        send(connectionFd, "1", strlen("1"), 0);
    }
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT == resSecurityCheck) {
        send(connectionFd, "2", strlen("2"), 0);
    }
}

void FaultLoggerDaemon::HandleSdkDumpRequest(int32_t connectionFd, FaultLoggerdRequest * request)
{
    DfxLogInfo("Receive dump request for pid:%d tid:%d.", request->pid, request->tid);
    FaultLoggerSdkDumpResp resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_PASS;
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
            DfxLogError("%s :: HandleSdkDumpRequest :: pid(%d) or resSecurityCheck(%d) fail.\n", \
                        FAULTLOGGERD_TAG.c_str(), request->pid, (int)resSecurityCheck);
            resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_REJECT;
            break;
        }

        if (faultLoggerPipeMap_->Get(request->pid) != nullptr) {
            resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_REPEAT;
            DfxLogError("%s :: pid(%d) is dumping, break.\n", FAULTLOGGERD_TAG.c_str(), request->pid);
            break;
        }
        faultLoggerPipeMap_->Set(request->pid);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
        // defined in out/hi3516dv300/obj/third_party/musl/intermidiates/linux/musl_src_ported/include/signal.h
        siginfo_t si = {
            .si_signo = SIGDUMP,
            .si_errno = 0,
            .si_code = request->sigCode,
            .si_value.sival_int = request->tid,
            .si_pid = request->callerPid,
            .si_uid = static_cast<uid_t>(request->callerTid)
        };
#pragma clang diagnostic pop
        // means we need dump all the threads in a process.
        if (request->tid == 0) {
            if (syscall(SYS_rt_sigqueueinfo, request->pid, si.si_signo, &si) != 0) {
                DfxLogError("Failed to SYS_rt_sigqueueinfo signal(%d), errno(%d).", si.si_signo, errno);
                resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_NOPROC;
                break;
            }
        } else {
            // means we need dump a specified thread
            if (syscall(SYS_rt_tgsigqueueinfo, request->pid, request->tid, si.si_signo, &si) != 0) {
                DfxLogError("Failed to SYS_rt_tgsigqueueinfo signal(%d), errno(%d).", si.si_signo, errno);
                resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_NOPROC;
                break;
            }
        }
    } while (false);

    switch (resSdkDump) {
        case FaultLoggerSdkDumpResp::SDK_DUMP_REJECT:
            send(connectionFd, "2", strlen("2"), 0);
            break;
        case FaultLoggerSdkDumpResp::SDK_DUMP_REPEAT:
            send(connectionFd, "3", strlen("3"), 0);
            break;
        case FaultLoggerSdkDumpResp::SDK_DUMP_NOPROC:
            send(connectionFd, "4", strlen("4"), 0);
            break;
        default:
            send(connectionFd, "1", strlen("1"), 0);
            break;
    }
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
        time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>\
            (std::chrono::system_clock::now().time_since_epoch()).count());
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

void FaultLoggerDaemon::AddEvent(int32_t epollFd, int32_t addFd, int32_t event)
{
    epoll_event ev;
    ev.events = event;
    ev.data.fd = addFd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, addFd, &ev);
}

void FaultLoggerDaemon::DelEvent(int32_t epollFd, int32_t delFd, int32_t event)
{
    epoll_event ev;
    ev.events = event;
    ev.data.fd = delFd;
    epoll_ctl(epollFd, EPOLL_CTL_DEL, delFd, &ev);
}
} // namespace HiviewDFX
} // namespace OHOS
