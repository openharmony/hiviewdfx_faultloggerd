/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include <fstream>
#include <securec.h>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "dfx_define.h"
#include "dfx_exception.h"
#include "dfx_log.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#include "string_printf.h"
#include "directory_ex.h"
#include "fault_logger_config.h"
#include "faultloggerd_socket.h"
#ifndef is_ohos_lite
#include "parameters.h"
#endif
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

namespace OHOS {
namespace HiviewDFX {
std::shared_ptr<FaultLoggerConfig> faultLoggerConfig_;
std::shared_ptr<FaultLoggerPipeMap> faultLoggerPipeMap_;

namespace {
constexpr int32_t MAX_CONNECTION = 30;
constexpr int32_t REQUEST_BUF_SIZE = 2048;
constexpr int32_t MAX_EPOLL_EVENT = 1024;
const int32_t FAULTLOG_FILE_PROP = 0640;

static constexpr uint32_t ROOT_UID = 0;
static constexpr uint32_t BMS_UID = 1000;
static constexpr uint32_t HIVIEW_UID = 1201;
static constexpr uint32_t HIDUMPER_SERVICE_UID = 1212;
static constexpr uint32_t FOUNDATION_UID = 5523;
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
        case (int32_t)FaultLoggerType::JS_RAW_SNAPSHOT:
            return "jsheap";
        case (int32_t)FaultLoggerType::JS_HEAP_LEAK_LIST:
            return "leaklist";
        case (int32_t)FaultLoggerType::LEAK_STACKTRACE:
            return "leakstack";
        case (int32_t)FaultLoggerType::FFRT_CRASH_LOG:
            return "ffrtlog";
        case (int32_t)FaultLoggerType::JIT_CODE_LOG:
            return "jitcode";
        default:
            return "unsupported";
    }
}

static bool CheckCallerUID(uint32_t callerUid)
{
    // If caller's is BMS / root or caller's uid/pid is validate, just return true
    if ((callerUid == BMS_UID) ||
        (callerUid == ROOT_UID) ||
        (callerUid == HIVIEW_UID) ||
        (callerUid == HIDUMPER_SERVICE_UID) ||
        (callerUid == FOUNDATION_UID)) {
        return true;
    }
    DFXLOG_WARN("%s :: CheckCallerUID :: Caller Uid(%d) is unexpectly.\n", FAULTLOGGERD_TAG.c_str(), callerUid);
    return false;
}
}

static void ReportExceptionToSysEvent(CrashDumpException& exception)
{
#ifndef HISYSEVENT_DISABLE
    std::string errMessage;
    if (exception.error == CRASH_DUMP_LOCAL_REPORT) {
        std::ifstream rfile;
        if (strlen(exception.message) == 0) {
            return;
        }
        rfile.open(exception.message, std::ios::binary | std::ios::ate);
        if (!rfile.is_open()) {
            return;
        }
        std::streamsize size = rfile.tellg();
        rfile.seekg(0, std::ios::beg);
        std::vector<char> buf(size);
        rfile.read(buf.data(), size);
        errMessage = std::string(buf.begin(), buf.end());
    } else {
        errMessage = exception.message;
    }
    HiSysEventWrite(
        HiSysEvent::Domain::RELIABILITY,
        "CPP_CRASH_EXCEPTION",
        HiSysEvent::EventType::FAULT,
        "PID", exception.pid,
        "UID", exception.uid,
        "HAPPEN_TIME", exception.time,
        "ERROR_CODE", exception.error,
        "ERROR_MSG", errMessage);
#endif
}

FaultLoggerDaemon::FaultLoggerDaemon()
{
#ifndef is_ohos_lite
    isBeta_ = OHOS::system::GetParameter("const.logsystem.versiontype", "false") == "beta";
#endif
}

int32_t FaultLoggerDaemon::StartServer()
{
    if (!CreateSockets()) {
        DFXLOG_ERROR("%s :: Failed to create faultloggerd sockets.", FAULTLOGGERD_TAG.c_str());
        CleanupSockets();
        return -1;
    }

    if (!InitEnvironment()) {
        DFXLOG_ERROR("%s :: Failed to init environment.", FAULTLOGGERD_TAG.c_str());
        CleanupSockets();
        return -1;
    }

    if (!CreateEventFd()) {
        DFXLOG_ERROR("%s :: Failed to create eventFd.", FAULTLOGGERD_TAG.c_str());
        CleanupSockets();
        return -1;
    }
    RemoveTempFileIfNeed();
    // loop in WaitForRequest
    WaitForRequest();
    CleanupEventFd();
    CleanupSockets();
    return 0;
}

void FaultLoggerDaemon::HandleAccept(int32_t epollFd, int32_t socketFd)
{
    DFX_TRACE_SCOPED("HandleAccept");
    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));

    int connectionFd = OHOS_TEMP_FAILURE_RETRY(accept(socketFd,
        reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize));
    if (connectionFd < 0) {
        DFXLOG_WARN("%s :: Failed to accept connection", FAULTLOGGERD_TAG.c_str());
        return;
    }

    AddEvent(eventFd_, connectionFd, EPOLLIN);
    connectionMap_[connectionFd] = socketFd;
}

#ifdef FAULTLOGGERD_FUZZER
bool FaultLoggerDaemon::HandleStaticForFuzzer(int32_t type, uint32_t callerUid)
{
    std::string str = GetRequestTypeName(type);
    bool ret = CheckCallerUID(callerUid);
    if (str == "unsupported" || !ret) {
        return false;
    } else {
        return true;
    }
}

void FaultLoggerDaemon::HandleRequestForFuzzer(int32_t epollFd, int32_t connectionFd,
                                               const FaultLoggerdRequest *requestConst, FaultLoggerdRequest *request)
{
    if (faultLoggerConfig_ == nullptr) {
        faultLoggerConfig_ = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
            LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    }
    HandleRequest(epollFd, connectionFd);
    HandleLogFileDesClientRequest(connectionFd, requestConst);
    HandlePrintTHilogClientRequest(connectionFd, request);
    HandlePermissionRequest(connectionFd, request);
    HandleSdkDumpRequest(connectionFd, request);
    HandleExceptionRequest(connectionFd, request);
}
#endif

static bool CheckReadRequest(ssize_t nread, ssize_t size)
{
    if (nread < 0) {
        DFXLOG_ERROR("%s :: Failed to read message", FAULTLOGGERD_TAG.c_str());
        return false;
    } else if (nread == 0) {
        DFXLOG_ERROR("%s :: Read null from request socket", FAULTLOGGERD_TAG.c_str());
        return false;
    } else if (nread != static_cast<long>(size)) {
        return false;
    }
    return true;
}

void FaultLoggerDaemon::HandleRequestByClientType(int32_t connectionFd, FaultLoggerdRequest* request)
{
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
        case static_cast<int32_t>(FaultLoggerClientType::REPORT_EXCEPTION_CLIENT):
            HandleExceptionRequest(connectionFd, request);
            break;
        default:
            DFXLOG_ERROR("%s :: unknown clientType(%d).\n", FAULTLOGGERD_TAG.c_str(), request->clientType);
            break;
        }
}

void FaultLoggerDaemon::HandleRequest(int32_t epollFd, int32_t connectionFd)
{
    if (epollFd < 0 || connectionFd < 3) { // 3: not allow fd = 0,1,2 because they are reserved by system
        DFXLOG_ERROR("%s :: HandleRequest recieved invalid fd parmeters.", FAULTLOGGERD_TAG.c_str());
        return;
    }

    std::vector<uint8_t> buf(REQUEST_BUF_SIZE, 0);
    do {
        ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(connectionFd, buf.data(), REQUEST_BUF_SIZE));
        if (CheckReadRequest(nread, sizeof(FaultLoggerdStatsRequest))) {
            HandleDumpStats(connectionFd, reinterpret_cast<FaultLoggerdStatsRequest *>(buf.data()));
            break;
        }
        if (!CheckReadRequest(nread, sizeof(FaultLoggerdRequest))) {
            break;
        }
        auto request = reinterpret_cast<FaultLoggerdRequest *>(buf.data());
        if (!CheckRequestCredential(connectionFd, request)) {
            break;
        }
        DFXLOG_DEBUG("%s :: clientType(%d).", FAULTLOGGERD_TAG.c_str(), request->clientType);
        HandleRequestByClientType(connectionFd, request);
    } while (false);
    DelEvent(eventFd_, connectionFd, EPOLLIN);
    connectionMap_.erase(connectionFd);
}

bool FaultLoggerDaemon::InitEnvironment()
{
    faultLoggerConfig_ = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    faultLoggerPipeMap_ = std::make_shared<FaultLoggerPipeMap>();

    if (!OHOS::ForceCreateDirectory(faultLoggerConfig_->GetLogFilePath())) {
        DFXLOG_ERROR("%s :: Failed to ForceCreateDirectory GetLogFilePath", FAULTLOGGERD_TAG.c_str());
        return false;
    }

    if (!OHOS::ForceCreateDirectory(faultLoggerConfig_->GetDebugLogFilePath())) {
        DFXLOG_ERROR("%s :: Failed to ForceCreateDirectory GetDebugLogFilePath", FAULTLOGGERD_TAG.c_str());
        return false;
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    return true;
}

void FaultLoggerDaemon::HandleDefaultClientRequest(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    DFX_TRACE_SCOPED("HandleDefaultClientRequest");
    int fd = CreateFileForRequest(request->type, request->pid, request->tid, request->time, false);
    if (fd < 0) {
        DFXLOG_ERROR("%s :: Failed to create log file, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
        return;
    }
    RecordFileCreation(request->type, request->pid);
    SendFileDescriptorToSocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandleLogFileDesClientRequest(int32_t connectionFd, const FaultLoggerdRequest * request)
{
    int fd = CreateFileForRequest(request->type, request->pid, request->tid, request->time, true);
    if (fd < 0) {
        DFXLOG_ERROR("%s :: Failed to create log file, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);

    close(fd);
}

void FaultLoggerDaemon::HandleExceptionRequest(int32_t connectionFd, FaultLoggerdRequest * request)
{
    if (OHOS_TEMP_FAILURE_RETRY(write(connectionFd,
        DAEMON_RESP.c_str(), DAEMON_RESP.length())) != static_cast<ssize_t>(DAEMON_RESP.length())) {
        DFXLOG_ERROR("%s :: Failed to write DAEMON_RESP.", FAULTLOGGERD_TAG.c_str());
    }

    CrashDumpException exception;
    (void)memset_s(&exception, sizeof(CrashDumpException), 0, sizeof(CrashDumpException));
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(connectionFd, &exception, sizeof(CrashDumpException)));
    exception.message[LINE_BUF_SIZE - 1] = '\0';
    if (!CheckReadRequest(nread, sizeof(CrashDumpException))) {
        return;
    }

    ReportExceptionToSysEvent(exception);
}

void FaultLoggerDaemon::HandleReadBuf(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
    FaultLoggerPipe2* faultLoggerPipe)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS != resSecurityCheck) {
        return;
    }
    if ((faultLoggerPipe->faultLoggerPipeBuf_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerPipeBuf_->GetReadFd();
    }
}

void FaultLoggerDaemon::HandleWriteBuf(int& fd, FaultLoggerPipe2* faultLoggerPipe)
{
    if ((faultLoggerPipe->faultLoggerPipeBuf_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerPipeBuf_->GetWriteFd();
    }
}

void FaultLoggerDaemon::HandleReadRes(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
    FaultLoggerPipe2* faultLoggerPipe)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS != resSecurityCheck) {
        return;
    }
    if ((faultLoggerPipe->faultLoggerPipeRes_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerPipeRes_->GetReadFd();
    }
}

void FaultLoggerDaemon::HandleWriteRes(int& fd, FaultLoggerPipe2* faultLoggerPipe)
{
    if ((faultLoggerPipe->faultLoggerPipeRes_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerPipeRes_->GetWriteFd();
    }
}

void FaultLoggerDaemon::HandleDelete(FaultLoggerdRequest* request)
{
    faultLoggerPipeMap_->Del(request->pid);
}

void FaultLoggerDaemon::HandleJsonReadBuf(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
    FaultLoggerPipe2* faultLoggerPipe)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS != resSecurityCheck) {
        return;
    }
    if ((faultLoggerPipe->faultLoggerJsonPipeBuf_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerJsonPipeBuf_->GetReadFd();
    }
}

void FaultLoggerDaemon::HandleJsonWriteBuf(int& fd, FaultLoggerPipe2* faultLoggerPipe)
{
    if ((faultLoggerPipe->faultLoggerJsonPipeBuf_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerJsonPipeBuf_->GetWriteFd();
    }
}

void FaultLoggerDaemon::HandleJsonReadRes(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
    FaultLoggerPipe2* faultLoggerPipe)
{
    FaultLoggerCheckPermissionResp resSecurityCheck = SecurityCheck(connectionFd, request);
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS != resSecurityCheck) {
        return;
    }
    if ((faultLoggerPipe->faultLoggerJsonPipeRes_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerJsonPipeRes_->GetReadFd();
    }
}

void FaultLoggerDaemon::HandleJsonWriteRes(int& fd, FaultLoggerPipe2* faultLoggerPipe)
{
    if ((faultLoggerPipe->faultLoggerJsonPipeRes_) != nullptr) {
        fd = faultLoggerPipe->faultLoggerJsonPipeRes_->GetWriteFd();
    }
}

void FaultLoggerDaemon::HandleRequestByPipeType(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
                                                FaultLoggerPipe2* faultLoggerPipe)
{
    switch (request->pipeType) {
        case (int32_t)FaultLoggerPipeType::PIPE_FD_READ_BUF: {
            HandleReadBuf(fd, connectionFd, request, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_WRITE_BUF: {
            HandleWriteBuf(fd, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_READ_RES: {
            HandleReadRes(fd, connectionFd, request, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_WRITE_RES: {
            HandleWriteRes(fd, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_DELETE: {
            HandleDelete(request);
            return;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF: {
            HandleJsonReadBuf(fd, connectionFd, request, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF: {
            HandleJsonWriteBuf(fd, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_JSON_READ_RES: {
            HandleJsonReadRes(fd, connectionFd, request, faultLoggerPipe);
            break;
        }
        case (int32_t)FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES: {
            HandleJsonWriteRes(fd, faultLoggerPipe);
            break;
        }
        default:
            DFXLOG_ERROR("%s :: unknown pipeType(%d).", FAULTLOGGERD_TAG.c_str(), request->pipeType);
            return;
    }
}

void FaultLoggerDaemon::HandlePipeFdClientRequest(int32_t connectionFd, FaultLoggerdRequest * request)
{
    DFX_TRACE_SCOPED("HandlePipeFdClientRequest");
    DFXLOG_DEBUG("%s :: pid(%d), pipeType(%d).", FAULTLOGGERD_TAG.c_str(), request->pid, request->pipeType);
    int fd = -1;
    FaultLoggerPipe2* faultLoggerPipe = faultLoggerPipeMap_->Get(request->pid);
    if (faultLoggerPipe == nullptr) {
        DFXLOG_ERROR("%s :: cannot find pipe fd for pid(%d).", FAULTLOGGERD_TAG.c_str(), request->pid);
        return;
    }
    HandleRequestByPipeType(fd, connectionFd, request, faultLoggerPipe);
    if (fd < 0) {
        DFXLOG_ERROR("%s :: Failed to get pipe fd, pipeType(%d)", FAULTLOGGERD_TAG.c_str(), request->pipeType);
        return;
    }
    SendFileDescriptorToSocket(connectionFd, fd);
}

void FaultLoggerDaemon::HandlePrintTHilogClientRequest(int32_t const connectionFd, FaultLoggerdRequest * request)
{
    char buf[LINE_BUF_SIZE] = {0};

    if (OHOS_TEMP_FAILURE_RETRY(write(connectionFd,
        DAEMON_RESP.c_str(), DAEMON_RESP.length())) != static_cast<ssize_t>(DAEMON_RESP.length())) {
        DFXLOG_ERROR("%s :: Failed to write DAEMON_RESP.", FAULTLOGGERD_TAG.c_str());
    }

    int nread = OHOS_TEMP_FAILURE_RETRY(read(connectionFd, buf, sizeof(buf) - 1));
    if (nread < 0) {
        DFXLOG_ERROR("%s :: Failed to read message, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
    } else if (nread == 0) {
        DFXLOG_ERROR("%s :: HandlePrintTHilogClientRequest :: Read null from request socket", FAULTLOGGERD_TAG.c_str());
    } else {
        DFXLOG_ERROR("%s", buf);
    }
}

FaultLoggerCheckPermissionResp FaultLoggerDaemon::SecurityCheck(int32_t connectionFd, FaultLoggerdRequest * request)
{
    FaultLoggerCheckPermissionResp resCheckPermission = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;

    struct ucred rcred;
    do {
        int optval = 1;
        if (OHOS_TEMP_FAILURE_RETRY(setsockopt(connectionFd,
            SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval))) == -1) {
            DFXLOG_ERROR("%s :: setsockopt SO_PASSCRED error, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
            break;
        }

        if (OHOS_TEMP_FAILURE_RETRY(write(connectionFd, DAEMON_RESP.c_str(), DAEMON_RESP.length())) !=
            static_cast<ssize_t>(DAEMON_RESP.length())) {
            DFXLOG_ERROR("%s :: Failed to write DAEMON_RESP, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
            break;
        }

        if (!RecvMsgCredFromSocket(connectionFd, &rcred)) {
            DFXLOG_ERROR("%s :: Recv msg ucred error, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
            break;
        }

        request->uid = rcred.uid;
        request->callerPid = static_cast<int32_t>(rcred.pid);

        auto it = connectionMap_.find(connectionFd);
        if (it == connectionMap_.end()) {
            break;
        }

        if (it->second == sdkdumpSocketFd_) {
            resCheckPermission = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS;
            break;
        }

        bool res = CheckCallerUID(request->uid);
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
        OHOS_TEMP_FAILURE_RETRY(send(connectionFd, "1", strlen("1"), 0));
    }
    if (FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT == resSecurityCheck) {
        OHOS_TEMP_FAILURE_RETRY(send(connectionFd, "2", strlen("2"), 0));
    }
}

void FaultLoggerDaemon::HandleSdkDumpRequest(int32_t connectionFd, FaultLoggerdRequest * request)
{
    DFX_TRACE_SCOPED("HandleSdkDumpRequest");
    DFXLOG_INFO("Receive dump request for pid:%d tid:%d.", request->pid, request->tid);
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
            DFXLOG_ERROR("%s :: HandleSdkDumpRequest :: pid(%d) or resSecurityCheck(%d) fail.\n", \
                FAULTLOGGERD_TAG.c_str(), request->pid, (int)resSecurityCheck);
            resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_REJECT;
            break;
        }
        DFXLOG_INFO("Sdk dump pid(%d) request pass permission verification.", request->pid);
        if (IsCrashed(request->pid)) {
            resSdkDump = FaultLoggerSdkDumpResp::SDK_PROCESS_CRASHED;
            DFXLOG_WARN("%s :: pid(%d) has been crashed, break.\n", FAULTLOGGERD_TAG.c_str(), request->pid);
            break;
        }
        if (faultLoggerPipeMap_->Check(request->pid, request->time)) {
            resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_REPEAT;
            DFXLOG_ERROR("%s :: pid(%d) is dumping, break.\n", FAULTLOGGERD_TAG.c_str(), request->pid);
            break;
        }
        faultLoggerPipeMap_->Set(request->pid, request->time, request->isJson);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
        // defined in out/hi3516dv300/obj/third_party/musl/intermidiates/linux/musl_src_ported/include/signal.h
        siginfo_t si {0};
        si.si_signo = SIGDUMP;
        si.si_errno = 0;
        si.si_value.sival_int = request->tid;
        if (request->tid == 0 && sizeof(void *) == 8) { // 8 : platform 64
            si.si_value.sival_ptr = reinterpret_cast<void *>(request->endTime | (1ULL << 63)); // 63 : platform 64
        }
        si.si_code = request->sigCode;
        si.si_pid = request->callerPid;
        si.si_uid = static_cast<uid_t>(request->callerTid);
#pragma clang diagnostic pop
        /*
         * means we need dump all the threads in a process
         * --------
         * Accroding to the linux manual, A process-directed signal may be delivered to any one of the
         * threads that does not currently have the signal blocked.
         */
        if (syscall(SYS_rt_sigqueueinfo, request->pid, si.si_signo, &si) != 0) {
            DFXLOG_ERROR("Failed to SYS_rt_sigqueueinfo signal(%d), errno(%d).", si.si_signo, errno);
            resSdkDump = FaultLoggerSdkDumpResp::SDK_DUMP_NOPROC;
        }
    } while (false);
    auto retMsg = std::to_string(resSdkDump);
    if (OHOS_TEMP_FAILURE_RETRY(send(connectionFd, retMsg.data(), retMsg.length(), 0)) !=
        static_cast<ssize_t>(retMsg.length())) {
        DFXLOG_ERROR("Failed to send result message to client, errno(%d).", errno);
    }
}

void FaultLoggerDaemon::RecordFileCreation(int32_t type, int32_t pid)
{
    if (type == static_cast<int32_t>(FaultLoggerType::CPP_CRASH)) {
        ClearTimeOutRecords();
        crashTimeMap_[pid] = time(nullptr);
    }
}

void FaultLoggerDaemon::ClearTimeOutRecords()
{
    constexpr int validTime = 8;
    auto currentTime = time(nullptr);
    for (auto it = crashTimeMap_.begin(); it != crashTimeMap_.end();) {
        if ((it->second + validTime) <= currentTime) {
            crashTimeMap_.erase(it++);
        } else {
            it++;
        }
    }
}

bool FaultLoggerDaemon::IsCrashed(int32_t pid)
{
    DFX_TRACE_SCOPED("IsCrashed");
    ClearTimeOutRecords();
    return crashTimeMap_.find(pid) != crashTimeMap_.end();
}

int32_t FaultLoggerDaemon::CreateFileForRequest(int32_t type, int32_t pid, int32_t tid,
    uint64_t time, bool debugFlag) const
{
    RemoveTempFileIfNeed();
    std::string typeStr = GetRequestTypeName(type);
    if (typeStr == "unsupported") {
        DFXLOG_ERROR("Unsupported request type(%d)", type);
        return -1;
    }

    std::string folderPath = "";
    if (debugFlag == false) {
        folderPath = faultLoggerConfig_->GetLogFilePath();
    } else {
        folderPath = faultLoggerConfig_->GetDebugLogFilePath();
    }

    if (time == 0) {
        time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>\
            (std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::string ss = folderPath + "/" + typeStr + "-" + std::to_string(pid);
    if (type == FaultLoggerType::JS_HEAP_SNAPSHOT || type == FaultLoggerType::JS_RAW_SNAPSHOT) {
        ss += "-" + std::to_string(tid);
    }
    ss += "-" + std::to_string(time);
    if (type == FaultLoggerType::JS_RAW_SNAPSHOT) {
        ss += ".rawheap";
    }
    const std::string path = ss;
    DFXLOG_INFO("%s :: file path(%s).\n", FAULTLOGGERD_TAG.c_str(), path.c_str());
    if (!VerifyFilePath(path, VALID_FILE_PATH)) {
        DFXLOG_ERROR("%s :: Open %s fail, please check it under valid path.\n", FAULTLOGGERD_TAG.c_str(), path.c_str());
        return -1;
    }
    int32_t fd = OHOS_TEMP_FAILURE_RETRY(open(path.c_str(), O_RDWR | O_CREAT, FAULTLOG_FILE_PROP));
    if (fd != -1) {
        if (!ChangeModeFile(path, FAULTLOG_FILE_PROP)) {
            DFXLOG_ERROR("%s :: Failed to ChangeMode CreateFileForRequest", FAULTLOGGERD_TAG.c_str());
        }
    }
    return fd;
}

void FaultLoggerDaemon::RemoveTempFileIfNeed() const
{
    int maxFileCount = 50;
    int currentLogCounts = 0;

    std::string logFilePath = faultLoggerConfig_->GetLogFilePath();
    std::vector<std::string> files;
    OHOS::GetDirFiles(logFilePath, files);
    constexpr uint64_t maxFileSize = 1lu << 31; // 2GB
    if (!isBeta_ && OHOS::GetFolderSize(logFilePath) > maxFileSize) {
        DFXLOG_ERROR("%s :: current file size is over limit, clear all files", FAULTLOGGERD_TAG.c_str());
        std::for_each(files.begin(), files.end(), OHOS::RemoveFile);
        return;
    }
    currentLogCounts = (int)files.size();
    maxFileCount = faultLoggerConfig_->GetLogFileMaxNumber();
    if (currentLogCounts < maxFileCount) {
        return;
    }

    std::sort(files.begin(), files.end(),
        [](const std::string& lhs, const std::string& rhs) -> int {
        auto lhsSplitPos = lhs.find_last_of("-");
        auto rhsSplitPos = rhs.find_last_of("-");
        if (lhsSplitPos == std::string::npos || rhsSplitPos == std::string::npos) {
            return lhs.compare(rhs) > 0;
        }

        return lhs.substr(lhsSplitPos).compare(rhs.substr(rhsSplitPos)) > 0;
    });

    time_t currentTime = static_cast<time_t>(time(nullptr));
    if (currentTime <= 0) {
        DFXLOG_ERROR("%s :: currentTime is less than zero CreateFileForRequest", FAULTLOGGERD_TAG.c_str());
    }

    constexpr int deleteNum = 1;
    int startIndex = maxFileCount > deleteNum ? maxFileCount - deleteNum : maxFileCount;
    for (unsigned int index = (unsigned int)startIndex; index < files.size(); index++) {
        struct stat st;
        int err = stat(files[index].c_str(), &st);
        if (err != 0) {
            DFXLOG_ERROR("%s :: Get log stat failed, errno(%d).", FAULTLOGGERD_TAG.c_str(), errno);
        } else {
            if ((currentTime - st.st_mtime) <= DAEMON_REMOVE_FILE_TIME_S) {
                continue;
            }
        }

        OHOS::RemoveFile(files[index]);
        DFXLOG_DEBUG("%s :: Now we rm file(%s) as max log number exceeded.", \
            FAULTLOGGERD_TAG.c_str(), files[index].c_str());
    }
}

void FaultLoggerDaemon::AddEvent(int32_t epollFd, int32_t addFd, uint32_t event)
{
    epoll_event ev;
    ev.events = event;
    ev.data.fd = addFd;
    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, addFd, &ev);
    if (ret < 0) {
        DFXLOG_WARN("%s :: Failed to epoll ctl add Fd(%d), errno(%d)", FAULTLOGGERD_TAG.c_str(), addFd, errno);
    }
}

void FaultLoggerDaemon::DelEvent(int32_t epollFd, int32_t delFd, uint32_t event)
{
    epoll_event ev;
    ev.events = event;
    ev.data.fd = delFd;
    int ret = epoll_ctl(epollFd, EPOLL_CTL_DEL, delFd, &ev);
    if (ret < 0) {
        DFXLOG_WARN("%s :: Failed to epoll ctl del Fd(%d), errno(%d)", FAULTLOGGERD_TAG.c_str(), delFd, errno);
    }
    close(delFd);
}

bool FaultLoggerDaemon::CheckRequestCredential(int32_t connectionFd, FaultLoggerdRequest* request)
{
    if (request == nullptr) {
        return false;
    }

    auto it = connectionMap_.find(connectionFd);
    if (it == connectionMap_.end()) {
        DFXLOG_ERROR("%s : Failed to find fd:%d, map size:%zu", FAULTLOGGERD_TAG.c_str(),
            connectionFd, connectionMap_.size());
        return false;
    }

    if (it->second == crashSocketFd_ || it->second == sdkdumpSocketFd_) {
        // only processdump use this socket
        return true;
    }

    struct ucred creds = {};
    socklen_t credSize = sizeof(creds);
    int err = getsockopt(connectionFd, SOL_SOCKET, SO_PEERCRED, &creds, &credSize);
    if (err != 0) {
        DFXLOG_ERROR("%s :: Failed to CheckRequestCredential, errno(%d)", FAULTLOGGERD_TAG.c_str(), errno);
        return false;
    }

    if (CheckCallerUID(creds.uid)) {
        return true;
    }

    bool isCredentialMatched = (creds.pid == request->pid);
    if (request->clientType == (int32_t)REPORT_EXCEPTION_CLIENT) {
        isCredentialMatched = (creds.uid == request->uid);   /* check uid when report exception */
    }
    if (!isCredentialMatched) {
        DFXLOG_WARN("Failed to check request credential request:%d:%d cred:%d:%d fd:%d:%d",
            request->pid, request->uid, creds.pid, creds.uid, it->second, crashSocketFd_);
    }
    return isCredentialMatched;
}

bool FaultLoggerDaemon::CreateSockets()
{
    if (!StartListen(defaultSocketFd_, SERVER_SOCKET_NAME, MAX_CONNECTION)) {
        return false;
    }

    if (!StartListen(crashSocketFd_, SERVER_CRASH_SOCKET_NAME, MAX_CONNECTION)) {
        close(defaultSocketFd_);
        defaultSocketFd_ = -1;
        return false;
    }
#ifndef is_ohos_lite
    if (!StartListen(sdkdumpSocketFd_, SERVER_SDKDUMP_SOCKET_NAME, MAX_CONNECTION)) {
        close(defaultSocketFd_);
        defaultSocketFd_ = -1;
        close(crashSocketFd_);
        crashSocketFd_ = -1;
        return false;
    }
#endif
    return true;
}

void FaultLoggerDaemon::CleanupSockets()
{
    if (defaultSocketFd_ >= 0) {
        close(defaultSocketFd_);
        defaultSocketFd_ = -1;
    }

    if (crashSocketFd_ >= 0) {
        close(crashSocketFd_);
        crashSocketFd_ = -1;
    }

    if (sdkdumpSocketFd_ >= 0) {
        close(sdkdumpSocketFd_);
        sdkdumpSocketFd_ = -1;
    }
}

bool FaultLoggerDaemon::CreateEventFd()
{
    eventFd_ = epoll_create(MAX_EPOLL_EVENT);
    if (eventFd_ < 0) {
        return false;
    }
    return true;
}

void FaultLoggerDaemon::WaitForRequest()
{
    AddEvent(eventFd_, defaultSocketFd_, EPOLLIN);
    AddEvent(eventFd_, crashSocketFd_, EPOLLIN);
    AddEvent(eventFd_, sdkdumpSocketFd_, EPOLLIN);
    epoll_event events[MAX_CONNECTION];
    DFXLOG_DEBUG("%s :: %s: start epoll wait.", FAULTLOGGERD_TAG.c_str(), __func__);
    do {
        int epollNum = OHOS_TEMP_FAILURE_RETRY(epoll_wait(eventFd_, events, MAX_CONNECTION, -1));
        if (epollNum < 0) {
            DFXLOG_ERROR("%s :: %s: epoll wait error, errno(%d).", FAULTLOGGERD_TAG.c_str(), __func__, errno);
            continue;
        }
        for (int i = 0; i < epollNum; i++) {
            if (!(events[i].events & EPOLLIN)) {
                DFXLOG_WARN("%s :: %s: epoll event(%d) error.", FAULTLOGGERD_TAG.c_str(), __func__, events[i].events);
                continue;
            }

            int fd = events[i].data.fd;
            if (fd == defaultSocketFd_ || fd == crashSocketFd_ || fd == sdkdumpSocketFd_) {
                HandleAccept(eventFd_, fd);
            } else {
                HandleRequest(eventFd_, fd);
            }
        }
    } while (true);
}

void FaultLoggerDaemon::CleanupEventFd()
{
    DelEvent(eventFd_, defaultSocketFd_, EPOLLIN);
    DelEvent(eventFd_, crashSocketFd_, EPOLLIN);
    DelEvent(eventFd_, sdkdumpSocketFd_, EPOLLIN);

    if (eventFd_ > 0) {
        close(eventFd_);
        eventFd_ = -1;
    }
}

std::string GetElfName(FaultLoggerdStatsRequest* request)
{
    if (request == nullptr || strlen(request->callerElf) > NAME_BUF_LEN) {
        return "";
    }

    std::string ss = StringPrintf("%s(%p)", request->callerElf, reinterpret_cast<void *>(request->offset));
    return ss;
}

void FaultLoggerDaemon::HandleDumpStats(int32_t connectionFd, FaultLoggerdStatsRequest* request)
{
    DFXLOG_INFO("%s :: %s: HandleDumpStats", FAULTLOGGERD_TAG.c_str(), __func__);
    size_t index = 0;
    bool hasRecord = false;
    for (index = 0; index < stats_.size(); index++) {
        if (stats_[index].pid == request->pid) {
            hasRecord = true;
            break;
        }
    }

    DumpStats stats;
    if (request->type == PROCESS_DUMP && !hasRecord) {
        stats.pid = request->pid;
        stats.signalTime = request->signalTime;
        stats.processdumpStartTime = request->processdumpStartTime;
        stats.processdumpFinishTime = request->processdumpFinishTime;
        stats.targetProcessName = request->targetProcess;
        stats_.emplace_back(stats);
    } else if (request->type == DUMP_CATCHER && hasRecord) {
        stats_[index].requestTime = request->requestTime;
        stats_[index].dumpCatcherFinishTime = request->dumpCatcherFinishTime;
        stats_[index].callerElfName = GetElfName(request);
        stats_[index].callerProcessName = request->callerProcess;
        stats_[index].result = request->result;
        stats_[index].summary = request->summary;
        ReportDumpStats(stats_[index]);
        stats_.erase(stats_.begin() + index);
    } else if (request->type == DUMP_CATCHER) {
        stats.pid = request->pid;
        stats.requestTime = request->requestTime;
        stats.dumpCatcherFinishTime = request->dumpCatcherFinishTime;
        stats.callerElfName = GetElfName(request);
        stats.result = request->result;
        stats.callerProcessName = request->callerProcess;
        stats.summary = request->summary;
        stats.targetProcessName = request->targetProcess;
        ReportDumpStats(stats);
    }
    RemoveTimeoutDumpStats();
}

void FaultLoggerDaemon::RemoveTimeoutDumpStats()
{
    const uint64_t timeout = 10000;
    uint64_t now = GetTimeMilliSeconds();
    for (auto it = stats_.begin(); it != stats_.end();) {
        if (((now > it->signalTime) && (now - it->signalTime > timeout)) ||
            (now <= it->signalTime)) {
            it = stats_.erase(it);
        } else {
            ++it;
        }
    }
}

void FaultLoggerDaemon::ReportDumpStats(const DumpStats& stat)
{
#ifndef HISYSEVENT_DISABLE
    HiSysEventWrite(
        HiSysEvent::Domain::HIVIEWDFX,
        "DUMP_CATCHER_STATS",
        HiSysEvent::EventType::STATISTIC,
        "CALLER_PROCESS_NAME", stat.callerProcessName,
        "CALLER_FUNC_NAME", stat.callerElfName,
        "TARGET_PROCESS_NAME", stat.targetProcessName,
        "RESULT", stat.result,
        "SUMMARY", stat.summary, // we need to parse summary when interface return false
        "PID", stat.pid,
        "REQUEST_TIME", stat.requestTime,
        "OVERALL_TIME", stat.dumpCatcherFinishTime - stat.requestTime,
        "SIGNAL_TIME", stat.signalTime - stat.requestTime,
        "DUMPER_START_TIME", stat.processdumpStartTime - stat.signalTime,
        "UNWIND_TIME", stat.processdumpFinishTime - stat.processdumpStartTime);
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
