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

/* This files contains process dump hrader. */

#ifndef DFX_RING_BUFFER_WRAPPER_H
#define DFX_RING_BUFFER_WRAPPER_H

#include <cinttypes>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "dfx_define.h"
#include "dfx_ring_buffer.h"
#include "nocopyable.h"

typedef int (*RingBufferWriteFunc) (int32_t fd, const char *buf, const int len);

namespace OHOS {
namespace HiviewDFX {
class DfxRingBufferWrapper final {
public:
    static DfxRingBufferWrapper &GetInstance();
    ~DfxRingBufferWrapper() = default;

    void StartThread();
    void StopThread();

    void SetWriteBufFd(int32_t fd);
    void SetWriteFunc(RingBufferWriteFunc func);

    void AppendMsg(const std::string& msg);
    int AppendBuf(const char *format, ...);
private:
    static void LoopPrintRingBuffer();
    static int DefaultWrite(int32_t fd, const char *buf, const int len);

    DfxRingBufferWrapper() = default;
    DISALLOW_COPY_AND_MOVE(DfxRingBufferWrapper);

    RingBufferWriteFunc writeFunc_ = nullptr;

    std::thread thread_;
    DfxRingBuffer<BACK_TRACE_RING_BUFFER_SIZE, std::string> ringBuffer_;
    int32_t fd_ = -1;
    volatile bool hasFinished_ = false;

    static std::condition_variable printCV_;
    static std::mutex printMutex_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
