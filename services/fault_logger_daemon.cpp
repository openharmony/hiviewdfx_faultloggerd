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

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <csignal>
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

#include <directory_ex.h>
#include <file_ex.h>
#include <securec.h>

#include "dfx_log.h"
#include "fault_logger_config.h"
#include "fault_logger_secure.h"
#include "fault_logger_daemon.h"

namespace OHOS {
namespace HiviewDFX {
using FaultLoggerdRequest = struct FaultLoggerdRequest;
std::shared_ptr<FaultLoggerConfig> faultLoggerConfig_;
std::shared_ptr<FaultLoggerSecure> faultLoggerSecure_;

namespace {
constexpr int32_t MAX_CONNECTION = 30;
constexpr int32_t REQUEST_BUF_SIZE = 1024;
constexpr int32_t MSG_BUF_SIZE = 256;

const int32_t FAULTLOG_FILE_PROP = 0662;
const int32_t FAULTLOG_FOLDER_PROP = 0771;

static const std::string LOG_LABLE = "FaultLoggerd";

static const std::string DAEMON_RESP = "RESP:COMPLETE";
static const char FAULTLOGGERD_SOCK_PATH[] = "/dev/unix/socket/faultloggerd.server";

const int SIGDUMP = 35;
const int MINUS_ONE_THOUSAND = -1000;

static const int LOG_BUF_LEN = 1024;
static const int GC_TIME_US = 1000000;

static std::string GetRequestTypeName(int32_t type)
{
    switch (type) {
        case (int32_t)FaultLoggerType::CPP_CRASH:
            return "cppcrash";
        case (int32_t)FaultLoggerType::CPP_STACKTRACE:
            return "stacktrace";
        default:
            return "unsupported";
    }
}

static void SendFileDescriptorBySocket(int socket, int fd)
{
    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(fd))] = { 0 };
    char iovBase[] = "";
    struct iovec io = {
        .iov_base = reinterpret_cast<void *>(iovBase),
        .iov_len = 1
    };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg != nullptr) {
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    }

    *(reinterpret_cast<int *>(CMSG_DATA(cmsg))) = fd;
    msg.msg_controllen = CMSG_SPACE(sizeof(fd));
    if (sendmsg(socket, &msg, 0) < 0) {
        DfxLogError("Failed to send message");
    }
}

__attribute__((unused)) static int ReadFileDescriptorFromSocket(int socket)
{
    struct msghdr msg = { 0 };

    char msgBuffer[MSG_BUF_SIZE] = { 0 };
    struct iovec io = {
        .iov_base = msgBuffer,
        .iov_len = sizeof(msgBuffer)
    };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char ctlBuffer[MSG_BUF_SIZE] = { 0 };
    msg.msg_control = ctlBuffer;
    msg.msg_controllen = sizeof(ctlBuffer);

    if (recvmsg(socket, &msg, 0) < 0) {
        DfxLogError("%s :: Failed to receive message", LOG_LABLE.c_str());
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr) {
        return -1;
    }
    return *(reinterpret_cast<int *>(CMSG_DATA(cmsg)));
}
}

bool FaultLoggerDaemon::InitEnvironment()
{
    DfxLogInfo("%s :: %s Enter.", OHOS::HiviewDFX::LOG_LABLE.c_str(), __func__);

    faultLoggerConfig_ = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    faultLoggerSecure_ = std::make_shared<FaultLoggerSecure>();

    if (!OHOS::ForceCreateDirectory(faultLoggerConfig_->GetLogFilePath())) {
        DfxLogError("%s :: Failed to ForceCreateDirectory GetLogFilePath", LOG_LABLE.c_str());
        return false;
    }
    if (!OHOS::ChangeModeDirectory(faultLoggerConfig_->GetLogFilePath(), FAULTLOG_FOLDER_PROP)) {
        DfxLogError("%s :: Failed to ChangeModeDirectory GetLogFilePath", LOG_LABLE.c_str());
    }

    if (!OHOS::ForceCreateDirectory(faultLoggerConfig_->GetDebugLogFilePath())) {
        DfxLogError("%s :: Failed to ForceCreateDirectory GetDebugLogFilePath", LOG_LABLE.c_str());
        return false;
    }
    if (!OHOS::ChangeModeDirectory(faultLoggerConfig_->GetDebugLogFilePath(), FAULTLOG_FOLDER_PROP)) {
        DfxLogError("%s :: Failed to ChangeModeDirectory GetDebugLogFilePath", LOG_LABLE.c_str());
    }

    if (chmod(FAULTLOGGERD_SOCK_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH) < 0) {
        DfxLogError("%s :: Failed to chmod, %d", LOG_LABLE.c_str(), errno);
    }

    std::vector<std::string> files;
    OHOS::GetDirFiles(faultLoggerConfig_->GetLogFilePath(), files);
    currentLogCounts_ = (int32_t)files.size();

    DfxLogInfo("%s :: %s success finished.", OHOS::HiviewDFX::LOG_LABLE.c_str(), __func__);
    return true;
}

void FaultLoggerDaemon::HandleDefaultClientReqeust(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    RemoveTempFileIfNeed();

    int fd = CreateFileForRequest(request->type, request->pid, false);
    if (fd < 0) {
        DfxLogError("%s :: Failed to create log file", LOG_LABLE.c_str());
        return;
    }

    SendFileDescriptorBySocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandleLogFileDesClientReqeust(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    int fd = CreateFileForRequest(request->type, request->pid, true);
    if (fd < 0) {
        DfxLogError("%s :: Failed to create log file", LOG_LABLE.c_str());
        return;
    }

    SendFileDescriptorBySocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandlePrintTHilogClientReqeust(int32_t const connectionFd, FaultLoggerdRequest * request)
{
    char buf[LOG_BUF_LEN] = {0};

    if (write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length()) != (ssize_t)DAEMON_RESP.length()) {
        DfxLogError("%s :: Failed to write DAEMON_RESP.", LOG_LABLE.c_str());
    }

    int nread = read(connectionFd, buf, sizeof(buf) - 1);
    if (nread < 0) {
        DfxLogError("%s :: Failed to read message", LOG_LABLE.c_str());
    } else if (nread == 0) {
        DfxLogError("%s :: HandlePrintTHilogClientReqeust :: Read null from request socket", LOG_LABLE.c_str());
    } else {
        DfxLogError("%s", buf);
    }
}

FaultLoggerCheckPermissionResp FaultLoggerDaemon::SecurityCheck(int32_t connectionFd, FaultLoggerdRequest * request)
{
    struct iovec iov;
    int data;
    struct ucred rcred;
    struct msghdr msgh;
    int optval = 1;

    do {
        union {
            char buf[CMSG_SPACE(sizeof(struct ucred))];

            /* Space large enough to hold a 'ucred' structure */
            struct cmsghdr align;
        } controlMsg;

        if (setsockopt(connectionFd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) == -1) {
            break;
        }

        if (write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length()) != (ssize_t)DAEMON_RESP.length()) {
            DfxLogError("%s :: Failed to write DAEMON_RESP.", LOG_LABLE.c_str());
        }

        msgh.msg_name = nullptr;
        msgh.msg_namelen = 0;
        msgh.msg_iov = &iov;
        msgh.msg_iovlen = 1;
        iov.iov_base = &data;
        iov.iov_len = sizeof(data);

        msgh.msg_control = controlMsg.buf;
        msgh.msg_controllen = sizeof(controlMsg.buf);

        ssize_t nr = recvmsg(connectionFd, &msgh, 0);
        if (nr == -1) {
            break;
        }

        struct cmsghdr *cmsgp = nullptr;
        cmsgp = CMSG_FIRSTHDR(&msgh);
        if (cmsgp == nullptr) {
            break;
        }

        if (memcpy_s(&rcred, sizeof(rcred), CMSG_DATA(cmsgp), sizeof(struct ucred)) != 0) {
            break;
        }

        request->uid = (int32_t)rcred.uid;
        request->callerPid = (int32_t)rcred.pid;
        bool res = faultLoggerSecure_->CheckCallerUID((int)request->uid, request->pid);
        if (res) {
            return FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS;
        }
    } while (false);

    return FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;
}

void FaultLoggerDaemon::HandlePermissionReqeust(int32_t connectionFd, FaultLoggerdRequest * request)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    DfxLogInfo("%s :: resSecurityCheck(%d).\n", LOG_LABLE.c_str(), (int)resSecurityCheck);

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
    DfxLogInfo("%s :: resSecurityCheck(%d).\n", LOG_LABLE.c_str(), (int)resSecurityCheck);

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
            DfxLogError("%s :: HandleSdkDumpReqeust :: pid or resSecurityCheck fail.\n", LOG_LABLE.c_str());
            break;
        }

        int sig = SIGDUMP ;
        // defined in out/hi3516dv300/obj/third_party/musl/intermidiates/linux/musl_src_ported/include/signal.h
        siginfo_t si = {
            .si_signo = SIGDUMP,
            .si_errno = 0,
            .si_code = MINUS_ONE_THOUSAND,
            .si_pid = request->callerPid,
            .si_uid = request->callerTid
        };

// means we need dump all the threads in a process.
        if (request->tid == 0) {
            if (syscall(SYS_rt_sigqueueinfo, request->pid, sig, &si) != 0) {
                DfxLogError("Failed to SYS_rt_sigqueueinfo signal(%d), errno(%d).",
                    si.si_signo, errno);
                break;
            }
        } else {
// means we need dump a specified thread
            if (syscall(SYS_rt_tgsigqueueinfo, request->pid, request->tid, sig, &si) != 0) {
                DfxLogError("Failed to SYS_rt_tgsigqueueinfo signal(%d), errno(%d).",
                    si.si_signo, errno);
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

void FaultLoggerDaemon::HandleRequest(int32_t connectionFd)
{
    ssize_t nread = -1;
    char buf[REQUEST_BUF_SIZE] = {0};

    do {
        nread = read(connectionFd, buf, sizeof(buf));
        if (nread < 0) {
            DfxLogError("%s :: Failed to read message", LOG_LABLE.c_str());
            break;
        } else if (nread == 0) {
            DfxLogError("%s :: HandleRequest :: Read null from request socket", LOG_LABLE.c_str());
            break;
        } else if (nread != static_cast<long>(sizeof(FaultLoggerdRequest))) {
            DfxLogError("%s :: Unmatched request length", LOG_LABLE.c_str());
            break;
        }

        auto request = reinterpret_cast<FaultLoggerdRequest *>(buf);
        DfxLogInfo("%s :: clientType(%d), type(%d).\n", LOG_LABLE.c_str(),
            request->clientType, request->type);

        if (request->clientType == (int32_t)FaultLoggerClientType::DEFAULT_CLIENT) {
            HandleDefaultClientReqeust(connectionFd, request);
            break;
        } else if (request->clientType == (int32_t)FaultLoggerClientType::LOG_FILE_DES_CLIENT) {
            HandleLogFileDesClientReqeust(connectionFd, request);
            break;
        } else if (request->clientType == (int32_t)FaultLoggerClientType::PRINT_T_HILOG_CLIENT) {
            HandlePrintTHilogClientReqeust(connectionFd, request);
            break;
        } else if (request->clientType == (int32_t)FaultLoggerClientType::PERMISSION_CLIENT) {
            HandlePermissionReqeust(connectionFd, request);
            break;
        } else if (request->clientType == (int32_t)FaultLoggerClientType::SDK_DUMP_CLIENT) {
            HandleSdkDumpReqeust(connectionFd, request);
            break;
        } else {
            DfxLogError("%s :: unknown client just break and close socket.\n", LOG_LABLE.c_str());
            break;
        }
    } while (false);

    close(connectionFd);
}

int32_t FaultLoggerDaemon::CreateFileForRequest(int32_t type, int32_t pid, bool debugFlag) const
{
    if (type != (int32_t)FaultLoggerType::CPP_STACKTRACE && type != (int32_t)FaultLoggerType::CPP_CRASH) {
        DfxLogError("%s :: Unsupported request type", LOG_LABLE.c_str());
        return -1;
    }

    std::string filePath = "";
    if (debugFlag == false) {
        filePath = faultLoggerConfig_->GetLogFilePath();
    } else {
        filePath = faultLoggerConfig_->GetDebugLogFilePath();
    }

    std::stringstream crashTime;
    time_t t = static_cast<time_t>(time(nullptr));
    if (t <= 0) {
        DfxLogError("%s :: time is less than zero CreateFileForRequest", LOG_LABLE.c_str());
    }
    crashTime << "-" << t;
    std::string path = filePath + "/" + GetRequestTypeName(type) + "-" + std::to_string(pid) + crashTime.str();

    DfxLogInfo("%s :: file path(%s).\n", LOG_LABLE.c_str(), path.c_str());
    int32_t fd = open(path.c_str(), O_RDWR | O_CREAT, FAULTLOG_FILE_PROP);
    if (fd != -1) {
        if (!OHOS::ChangeModeFile(path, FAULTLOG_FILE_PROP)) {
            DfxLogError("%s :: Failed to ChangeMode CreateFileForRequest", LOG_LABLE.c_str());
        }
    }
    return fd;
}

void FaultLoggerDaemon::RemoveTempFileIfNeed()
{
    int maxFileCount = 50;

    std::vector<std::string> files;
    OHOS::GetDirFiles(faultLoggerConfig_->GetLogFilePath(), files);
    currentLogCounts_ = (int32_t)files.size();

    maxFileCount = faultLoggerConfig_->GetLogFileMaxNumber();
    if (currentLogCounts_ < maxFileCount) {
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

    int startIndex = maxFileCount / 2;
    for (unsigned int index = (unsigned int)startIndex; index < files.size(); index++) {
        OHOS::RemoveFile(files[index]);
    }
}

void FaultLoggerDaemon::GcZStatProcess(void)
{
    int status = -1;

    while (true) {
        // try to wait and clear the finished child process(Z status).
        waitpid(-1, &status, WNOHANG);
        usleep(GC_TIME_US);
    }
}

void FaultLoggerDaemon::LoopAcceptRequestAndFork(int socketFd)
{
    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));
    int connectionFd = -1;

    auto tGcZStatProcess = std::thread(&FaultLoggerDaemon::GcZStatProcess, this);
    tGcZStatProcess.detach();

    while (true) {
        if ((connectionFd = accept(socketFd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize)) < 0) {
            DfxLogError("%s :: Failed to accept connection", LOG_LABLE.c_str());
            continue;
        }
        DfxLogInfo("%s :: %s: accept: %d.", LOG_LABLE.c_str(), __func__, connectionFd);

        int childPid = fork();
        if (childPid < 0) {
            close(connectionFd);
            break;
        } else if (childPid > 0) {
            DfxLogInfo("%s :: %s: forked child pid: %d", LOG_LABLE.c_str(), __func__, childPid);
        } else if (childPid == 0) {
            HandleRequest(connectionFd);
            close(connectionFd);
            exit(0);
        }
        close(connectionFd);
        connectionFd = -1;
    }

    DfxLogInfo("%s :: %s: Exit.", LOG_LABLE.c_str(), __func__);
}
} // namespace HiviewDFX
} // namespace OHOS

int32_t StartServer(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    int socketFd = -1;

    if ((socketFd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DfxLogError("%s :: Failed to create socket", OHOS::HiviewDFX::LOG_LABLE.c_str());
        return -1;
    }

    struct sockaddr_un server;
    errno_t err = memset_s(&server, sizeof(server), 0, sizeof(server));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s server failed..", __func__);
    }
    server.sun_family = AF_LOCAL;
    if (strncpy_s(server.sun_path, sizeof(server.sun_path), OHOS::HiviewDFX::FAULTLOGGERD_SOCK_PATH,
        strlen(OHOS::HiviewDFX::FAULTLOGGERD_SOCK_PATH)) != 0) {
        DfxLogError("%s :: Failed to set sock path", OHOS::HiviewDFX::LOG_LABLE.c_str());
        close(socketFd);
        return -1;
    }

    unlink(OHOS::HiviewDFX::FAULTLOGGERD_SOCK_PATH);
    if (bind(socketFd, (struct sockaddr *)&server,
        offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path)) < 0) {
        DfxLogError("%s :: Failed to bind socket", OHOS::HiviewDFX::LOG_LABLE.c_str());
        close(socketFd);
        return -1;
    }

    if (listen(socketFd, OHOS::HiviewDFX::MAX_CONNECTION) < 0) {
        DfxLogError("%s :: Failed to listen socket", OHOS::HiviewDFX::LOG_LABLE.c_str());
        close(socketFd);
        return -1;
    }

    OHOS::HiviewDFX::FaultLoggerDaemon daemon;
    if (!daemon.InitEnvironment()) {
        DfxLogError("%s :: Failed to init environment", OHOS::HiviewDFX::LOG_LABLE.c_str());
        close(socketFd);
        return -1;
    }

    DfxLogInfo("%s :: %s: start loop accept.", OHOS::HiviewDFX::LOG_LABLE.c_str(), __func__);
    daemon.LoopAcceptRequestAndFork(socketFd);

    close(socketFd);
    DfxLogInfo("%s :: %s: Exit.", OHOS::HiviewDFX::LOG_LABLE.c_str(), __func__);
    return 0;
}
