/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "backtrace_local_thread.h"

#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"
#include "procinfo.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxBacktraceLocal"
}

BacktraceLocalThread::BacktraceLocalThread(int32_t tid, std::shared_ptr<Unwinder> unwinder)
    : tid_(tid), unwinder_(unwinder)
{
    frames_.clear();
}

BacktraceLocalThread::~BacktraceLocalThread()
{
    frames_.clear();
}

bool BacktraceLocalThread::Unwind(bool fast, size_t maxFrameNum, size_t skipFrameNum)
{
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool ret = false;

    if (unwinder_ == nullptr || tid_ < BACKTRACE_CURRENT_THREAD) {
        return ret;
    }

    if (tid_ == BACKTRACE_CURRENT_THREAD) {
        ret = unwinder_->UnwindLocal(false, fast, maxFrameNum, skipFrameNum + 1);
#ifdef __aarch64__
        if (fast) {
            Unwinder::GetLocalFramesByPcs(frames_, unwinder_->GetPcs());
        }
#endif
        if (frames_.empty()) {
            frames_ = unwinder_->GetFrames();
        }
        return ret;
    }

    ret = unwinder_->UnwindLocalWithTid(tid_, maxFrameNum, skipFrameNum + 1);
#ifdef __aarch64__
    Unwinder::GetLocalFramesByPcs(frames_, unwinder_->GetPcs());
#else
    frames_ = unwinder_->GetFrames();
#endif
    return ret;
}

void BacktraceLocalThread::SetFrames(const std::vector<DfxFrame>& frames)
{
    frames_ = frames;
}

const std::vector<DfxFrame>& BacktraceLocalThread::GetFrames() const
{
    return frames_;
}

std::string BacktraceLocalThread::GetFormattedStr(bool withThreadName)
{
    if (frames_.empty()) {
        return "";
    }

    std::string ss;
    if (withThreadName && (tid_ > 0)) {
        std::string threadName;
        // Tid:1676, Name:IPC_3_1676
        ReadThreadName(tid_, threadName);
        ss = "Tid:" + std::to_string(tid_) + ", Name:" + threadName + "\n";
    }
    ss += Unwinder::GetFramesStr(frames_);
    return ss;
}

bool BacktraceLocalThread::UnwindSupportMix(bool fast, size_t maxFrameNum, size_t skipFrameNum)
{
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool ret = false;

    if (unwinder_ == nullptr || tid_ < BACKTRACE_CURRENT_THREAD) {
        return ret;
    }
    if (tid_ == BACKTRACE_CURRENT_THREAD || tid_ == gettid()) {
        ret = unwinder_->UnwindLocal(false, fast, maxFrameNum, skipFrameNum + 1);
    } else {
        ret = unwinder_->UnwindLocalByOtherTid(tid_, fast, maxFrameNum, skipFrameNum + 1);
    }
#ifdef __aarch64__
    if (ret && fast) {
        Unwinder::GetFramesByPcs(frames_, unwinder_->GetPcs());
    }
#endif
    if (frames_.empty()) {
        frames_ = unwinder_->GetFrames();
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
