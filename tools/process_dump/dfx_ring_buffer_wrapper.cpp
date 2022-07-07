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

/* This files contains process dump main module. */

#include "dfx_ring_buffer_wrapper.h"

#include <cerrno>
#include <cinttypes>
#include <memory>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ucontext.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <securec.h>
#include <string>

#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {

std::condition_variable DfxRingBufferWrapper::printCV_;
std::mutex DfxRingBufferWrapper::printMutex_;

DfxRingBufferWrapper &DfxRingBufferWrapper::GetInstance()
{
    static DfxRingBufferWrapper ins;
    return ins;
}

void DfxRingBufferWrapper::LoopPrintRingBuffer()
{
    std::unique_lock<std::mutex> lck(printMutex_);
    while (true) {
        bool hasFinished = DfxRingBufferWrapper::GetInstance().hasFinished_;
        auto available = DfxRingBufferWrapper::GetInstance().ringBuffer_.Available();
        auto item = DfxRingBufferWrapper::GetInstance().ringBuffer_.Read(available);
        
        if (available != 0) {
            //DfxLogDebug("%s :: item.Length(%d)", __func__, item.Length());
            if (item.At(0).empty()) {
                DfxRingBufferWrapper::GetInstance().ringBuffer_.Skip(item.Length());
                continue;
            }

            for (unsigned int i = 0; i < item.Length(); i++) {
                //DfxLogDebug("%s :: [%d]print: %s\n", __func__, i, item.At(i).c_str());
                if (DfxRingBufferWrapper::GetInstance().fd_ > 0) {
                    if (DfxRingBufferWrapper::GetInstance().writeFunc_ != nullptr) {
                        DfxRingBufferWrapper::GetInstance().writeFunc_(DfxRingBufferWrapper::GetInstance().fd_, \
                            item.At(i).c_str(), item.At(i).length());
                    } else {
                        DfxRingBufferWrapper::GetInstance().DefaultWrite(DfxRingBufferWrapper::GetInstance().fd_, \
                            item.At(i).c_str(), item.At(i).length());
                    }
                }
            }
            DfxRingBufferWrapper::GetInstance().ringBuffer_.Skip(item.Length());
        } else {
            if (hasFinished) {
                DfxLogDebug("%s :: print finished, exit loop.\n", __func__);
                break;
            }

            printCV_.wait_for(lck, std::chrono::milliseconds(BACK_TRACE_RING_BUFFER_PRINT_WAIT_TIME_MS));
        }
    }
}

void DfxRingBufferWrapper::AppendMsg(std::string msg)
{
    DfxLogDebug("%s :: msg: %s", __func__, msg.c_str());
    ringBuffer_.Append(msg);
    printCV_.notify_one();
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
    if (ret <= 0) {
        DfxLogError("snprintf_s failed.");
    }

    AppendMsg(std::string(buf));
    return ret;
}

void DfxRingBufferWrapper::StartThread()
{
    hasFinished_ = false;
    thread_ = std::thread(DfxRingBufferWrapper::LoopPrintRingBuffer);
}

void DfxRingBufferWrapper::StopThread()
{
    printCV_.notify_one();
    if (!hasFinished_) {
        hasFinished_ = true;
        printCV_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
            DfxLogDebug("%s :: thread join", __func__);
        }
        close(fd_);
        fd_ = -1;
    }
}

void DfxRingBufferWrapper::SetWriteBufFd(int32_t fd)
{
    fd_ = fd;
}

void DfxRingBufferWrapper::SetWriteFunc(RingBufferWriteFunc func)
{
    writeFunc_ = func;
}

int DfxRingBufferWrapper::DefaultWrite(int32_t fd, const char *buf, const int len)
{
    WriteLog(-1, "%s", buf);
    return write(fd, buf, len);
}
} // namespace HiviewDFX
} // namespace OHOS
