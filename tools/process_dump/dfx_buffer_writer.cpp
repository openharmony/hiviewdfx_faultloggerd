/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#include "dfx_buffer_writer.h"

#include <securec.h>
#ifndef is_ohos_lite
#include <string_ex.h>
#endif // !is_ohos_lite
#include <unistd.h>

#include "crash_exception.h"
#include "dfx_log.h"
#include "dfx_define.h"
#include "dfx_exception.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "faultloggerd_client.h"

namespace OHOS {
namespace HiviewDFX {

DfxBufferWriter &DfxBufferWriter::GetInstance()
{
    static DfxBufferWriter ins;
    return ins;
}

void DfxBufferWriter::WriteMsg(const std::string& msg)
{
    if (writeFunc_ == nullptr) {
        writeFunc_ = DfxBufferWriter::DefaultWrite;
    }
    constexpr size_t step = 1024 * 1024;
    for (size_t i = 0; i < msg.size(); i += step) {
        size_t length = (i + step) < msg.size() ? step : msg.size() - i;
        writeFunc_(bufFd_.GetFd(), msg.substr(i, length).c_str(), length);
    }
}

bool DfxBufferWriter::WriteDumpRes(int32_t dumpRes)
{
    if (!resFd_) {
        return false;
    }
    ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(resFd_.GetFd(), &dumpRes, sizeof(dumpRes)));
    if (nwrite != static_cast<ssize_t>(sizeof(dumpRes))) {
        DFXLOGE("%{public}s write fail, err:%{public}d", __func__, errno);
        return false;
    }
    return true;
}

void DfxBufferWriter::WriteFormatMsg(const char *format, ...)
{
    char buf[LINE_BUF_SIZE] = {0};
    va_list args;
    va_start(args, format);
    int ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    va_end(args);
    if (ret < 0) {
        DFXLOGE("%{public}s :: vsnprintf_s failed, line: %{public}d.", __func__, __LINE__);
        return;
    }
    WriteMsg(std::string(buf));
}

void DfxBufferWriter::AppendBriefDumpInfo(const std::string& info)
{
    briefDumpInfo_.append(info);
}

void DfxBufferWriter::PrintBriefDumpInfo()
{
#ifndef is_ohos_lite
    if (briefDumpInfo_.empty()) {
        DFXLOGE("crash base info is empty");
    }
    std::vector<std::string> dumpInfoLines;

    OHOS::SplitStr(briefDumpInfo_, "\n", dumpInfoLines);
    for (const auto& dumpInfoLine : dumpInfoLines) {
        DFXLOGI("%{public}s", dumpInfoLine.c_str());
    }
#endif
}

void DfxBufferWriter::Finish()
{
    if (bufFd_ && fsync(bufFd_.GetFd()) == -1) {
        DFXLOGW("Failed to fsync fd.");
    }
}

void DfxBufferWriter::SetWriteResFd(SmartFd fd)
{
    if (!fd) {
        DFXLOGE("invalid res fd, failed to set fd.\n");
        return;
    }
    resFd_ = std::move(fd);
}

void DfxBufferWriter::SetWriteBufFd(SmartFd fd)
{
    if (!fd) {
        DFXLOGE("invalid buffer fd, failed to set fd.\n");
        return;
    }
    bufFd_ = std::move(fd);
}

void DfxBufferWriter::SetWriteFunc(BufferWriteFunc func)
{
    writeFunc_ = func;
}

int DfxBufferWriter::DefaultWrite(int32_t fd, const char *buf, const size_t len)
{
    if (buf == nullptr) {
        return -1;
    }
    if (fd > 0) {
        ssize_t writeSize = 0;
        int savedErrno = 0;
        constexpr size_t maxTryTimes = 1000;
        size_t tryTimes = 0;
        do {
            writeSize = write(fd, buf, len);
            savedErrno = errno;
            if (++tryTimes >= maxTryTimes) {
                DFXLOGW("Exceeding the maximum number of retries!");
                break;
            }
            if (writeSize == -1 && savedErrno == EAGAIN) {
                usleep(1000); // 1000 : sleep 1ms try again
            }
        } while (writeSize == -1 && (savedErrno == EINTR || savedErrno == EAGAIN));
        return writeSize;
    }
    return 0;
}

int DfxBufferWriter::GetFaultloggerdRequestType()
{
    switch (request_.siginfo.si_signo) {
        case SIGLEAK_STACK:
            switch (abs(request_.siginfo.si_code)) {
                case SIGLEAK_STACK_FDSAN:
                    FALLTHROUGH_INTENDED;
                case SIGLEAK_STACK_JEMALLOC:
                    FALLTHROUGH_INTENDED;
                case SIGLEAK_STACK_BADFD:
                    return FaultLoggerType::CPP_STACKTRACE;
                default:
                    return FaultLoggerType::LEAK_STACKTRACE;
            }
        case SIGDUMP:
            return FaultLoggerType::CPP_STACKTRACE;
        default:
            return FaultLoggerType::CPP_CRASH;
    }
}

bool DfxBufferWriter::InitBufferWriter(const ProcessDumpRequest& request)
{
    request_ = request;
    DFX_TRACE_SCOPED("InitBufferWriter");
    if (request_.type == ProcessDumpType::DUMP_TYPE_PIPE) {
        DFXLOGW("SIGPIPE need not request buffer fd.");
        return true;
    }
    SmartFd bufferFd;
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        int pipeWriteFd[] = { -1, -1 };
        if (RequestPipeFd(request_.pid, FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == 0) {
            bufferFd = SmartFd{pipeWriteFd[PIPE_BUF_INDEX]};
            DfxBufferWriter::GetInstance().SetWriteResFd(SmartFd{pipeWriteFd[PIPE_RES_INDEX]});
        }
    } else {
        struct FaultLoggerdRequest faultloggerdRequest{
            .pid = request_.pid,
            .type = GetFaultloggerdRequestType(),
            .tid = request_.tid,
            .time = request_.timeStamp
        };
        bufferFd = SmartFd {RequestFileDescriptorEx(&faultloggerdRequest)};
        if (!bufferFd) {
            DFXLOGW("Failed to request_ fd from faultloggerd.");
            ReportCrashException(CrashExceptionCode::CRASH_DUMP_EWRITEFD);
            bufferFd = SmartFd {CreateFileForCrash(request_.pid, request_.timeStamp)};
        }
    }

    bool rst{bufferFd};
    DfxBufferWriter::GetInstance().SetWriteBufFd(std::move(bufferFd));
    return rst;
}

int32_t DfxBufferWriter::CreateFileForCrash(int32_t pid, uint64_t time) const
{
    const std::string dirPath = "/log/crash";
    const std::string logFileType = "cppcrash";
    const int32_t logcrashFileProp = 0644; // 0640:-rw-r--r--
    if (access(dirPath.c_str(), F_OK) != 0) {
        DFXLOGE("%{public}s is not exist.", dirPath.c_str());
        return INVALID_FD;
    }
    RemoveFileIfNeed(dirPath);
    std::string logPath = dirPath + "/" + logFileType + "-" + std::to_string(pid) + "-" + std::to_string(time);
    int32_t fd = OHOS_TEMP_FAILURE_RETRY(open(logPath.c_str(), O_RDWR | O_CREAT, logcrashFileProp));
    if (fd == INVALID_FD) {
        DFXLOGE("create %{public}s failed, errno=%{public}d", logPath.c_str(), errno);
    } else {
        DFXLOGI("create crash path %{public}s succ.", logPath.c_str());
    }
    return fd;
}

void DfxBufferWriter::RemoveFileIfNeed(const std::string& dirPath) const
{
    const int maxFileCount = 5;
    std::vector<std::string> files;
    OHOS::GetDirFiles(dirPath, files);
    if (files.size() < maxFileCount) {
        return;
    }

    std::sort(files.begin(), files.end(),
        [](const std::string& lhs, const std::string& rhs) {
        auto lhsSplitPos = lhs.find_last_of("-");
        auto rhsSplitPos = rhs.find_last_of("-");
        if (lhsSplitPos == std::string::npos || rhsSplitPos == std::string::npos) {
            return lhs.compare(rhs) < 0;
        }
        return lhs.substr(lhsSplitPos).compare(rhs.substr(rhsSplitPos)) < 0;
    });

    int deleteNum = static_cast<int>(files.size()) - (maxFileCount - 1);
    for (int index = 0; index < deleteNum; index++) {
        DFXLOGI("Now we delete file(%{public}s) due to exceed file max count.", files[index].c_str());
        OHOS::RemoveFile(files[index]);
    }
}

} // namespace HiviewDFX
} // namespace OHOS
