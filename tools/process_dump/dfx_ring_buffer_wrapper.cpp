/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

void DfxRingBufferWrapper::LoopPrintRingBuffer()
{
    if (DfxRingBufferWrapper::GetInstance().writeFunc_ == nullptr) {
        DfxRingBufferWrapper::GetInstance().writeFunc_ = DfxRingBufferWrapper::DefaultWrite;
    }

    std::unique_lock<std::mutex> lck(printMutex_);
    while (true) {
        auto available = DfxRingBufferWrapper::GetInstance().ringBuffer_.Available();
        auto item = DfxRingBufferWrapper::GetInstance().ringBuffer_.Read(available);

        if (available != 0) {
            if (item.At(0).empty()) {
                DfxRingBufferWrapper::GetInstance().ringBuffer_.Skip(item.Length());
                continue;
            }

            for (unsigned int i = 0; i < item.Length(); i++) {
                DfxRingBufferWrapper::GetInstance().writeFunc_(DfxRingBufferWrapper::GetInstance().fd_, \
                    item.At(i).c_str(), item.At(i).length());
            }
            DfxRingBufferWrapper::GetInstance().ringBuffer_.Skip(item.Length());
        } else {
            if (DfxRingBufferWrapper::GetInstance().hasFinished_) {
                DFXLOG_DEBUG("%s :: print finished, exit loop.\n", __func__);
                break;
            }

            printCV_.wait_for(lck, std::chrono::milliseconds(BACK_TRACE_RING_BUFFER_PRINT_WAIT_TIME_MS));
        }
    }
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
    char buf[LOG_BUF_LEN] = {0};
    (void)memset_s(&buf, sizeof(buf), 0, sizeof(buf));
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    va_end(args);
    if (ret < 0) {
        DFXLOG_ERROR("%s :: vsnprintf_s failed, line: %d.", __func__, __LINE__);
    }

    AppendMsg(std::string(buf));
    return ret;
}

void DfxRingBufferWrapper::StartThread()
{
    hasFinished_ = false;
}

void DfxRingBufferWrapper::StopThread()
{
    if (fd_ != INVALID_FD) {
        close(fd_);
    }
    fd_ = INVALID_FD;
}

void DfxRingBufferWrapper::SetWriteBufFd(int32_t fd)
{
    fd_ = fd;
    if (fd_ < 0) {
        DFXLOG_WARN("%s :: Failed to set fd.\n", __func__);
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
        return write(fd, buf, len);
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
