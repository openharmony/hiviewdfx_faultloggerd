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

#include "dfx_ring_buffer_wrapper.h"

#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_logger.h"

namespace OHOS {
namespace HiviewDFX {
static const int32_t INVALID_FD = -1;
static const int32_t UNUSED_FD = -2;
std::condition_variable DfxRingBufferWrapper::printCV_;
std::mutex DfxRingBufferWrapper::printMutex_;

DfxRingBufferWrapper &DfxRingBufferWrapper::GetInstance()
{
    static DfxRingBufferWrapper ins;
    return ins;
}

void DfxRingBufferWrapper::AppendMsg(const std::string& msg)
{
    if (writeFunc_ == nullptr) {
        writeFunc_ = DfxRingBufferWrapper::DefaultWrite;
    }
    writeFunc_(fd_, msg.c_str(), msg.length());
}

int DfxRingBufferWrapper::AppendBuf(const char *format, ...)
{
    int ret = -1;
    char buf[LINE_BUF_SIZE] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    va_end(args);
    if (ret < 0) {
        DFXLOGE("%{public}s :: vsnprintf_s failed, line: %{public}d.", __func__, __LINE__);
    }

    AppendMsg(std::string(buf));
    return ret;
}

static std::vector<std::string> SplitDumpInfo(const std::string& dumpInfo, const std::string& sepator)
{
    std::vector<std::string> result;
    if (dumpInfo.empty() || sepator.empty()) {
        return result;
    }
    size_t begin = 0;
    size_t pos = dumpInfo.find(sepator, begin);
    while (pos != std::string::npos) {
        result.push_back(dumpInfo.substr(begin, pos - begin));

        begin = pos + sepator.size();
        pos = dumpInfo.find(sepator, begin);
    }
    if (begin < dumpInfo.size()) {
        result.push_back(dumpInfo.substr(begin));
    }
    return result;
}

void DfxRingBufferWrapper::AppendBaseInfo(const std::string& info)
{
    crashBaseInfo_.emplace_back(info);
}

void DfxRingBufferWrapper::PrintBaseInfo()
{
    if (crashBaseInfo_.empty()) {
        DFXLOGE("crash base info is empty");
    }
    for (const auto& item : crashBaseInfo_) {
        std::vector<std::string> itemVec = SplitDumpInfo(item, "\n");
        for (size_t i = 0; i < itemVec.size(); i++) {
            DFXLOGI("%{public}s", itemVec[i].c_str());
        }
    }
}

void DfxRingBufferWrapper::StartThread()
{
    hasFinished_ = false;
}

void DfxRingBufferWrapper::StopThread()
{
    if (fd_ != INVALID_FD) {
        if (fsync(fd_) == -1) {
            DFXLOGW("Failed to fsync fd.");
        }
        fdsan_close_with_tag(fd_, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));
    }
    fd_ = INVALID_FD;
}

void DfxRingBufferWrapper::SetWriteBufFd(int32_t fd)
{
    fd_ = fd;
    if (fd_ < 0) {
        DFXLOGW("%{public}s :: Failed to set fd.\n", __func__);
    }
}

void DfxRingBufferWrapper::SetWriteFunc(RingBufferWriteFunc func)
{
    writeFunc_ = func;
}

int DfxRingBufferWrapper::DefaultWrite(int32_t fd, const char *buf, const int len)
{
    if (buf == nullptr) {
        return -1;
    }
    WriteLog(UNUSED_FD, "%s", buf);
    if (fd > 0) {
        return OHOS_TEMP_FAILURE_RETRY(write(fd, buf, len));
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
