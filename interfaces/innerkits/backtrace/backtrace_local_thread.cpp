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
#include <sstream>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_frame_formatter.h"
#include "dfx_json_formatter.h"
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
    maxFrameNums_ = DEFAULT_MAX_FRAME_NUM;
    frames_.clear();
}

BacktraceLocalThread::~BacktraceLocalThread()
{
    frames_.clear();
}

bool BacktraceLocalThread::Unwind(size_t skipFrameNum, bool fast)
{
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool ret = false;

    if (tid_ < BACKTRACE_CURRENT_THREAD) {
        return ret;
    }

    if (tid_ == BACKTRACE_CURRENT_THREAD) {
        ret = unwinder_->UnwindLocal(false, fast, maxFrameNums_, skipFrameNum + 1);
        if (fast) {
            Unwinder::GetLocalFramesByPcs(frames_, unwinder_->GetPcs());
        } else {
            frames_ = unwinder_->GetFrames();
        }
        return ret;
    }

    ret = unwinder_->UnwindLocalWithTid(tid_, maxFrameNums_, skipFrameNum + 1);
#ifdef __aarch64__
    Unwinder::GetLocalFramesByPcs(frames_, unwinder_->GetPcs());
#else
    frames_ = unwinder_->GetFrames();
#endif
    return ret;
}

const std::vector<DfxFrame>& BacktraceLocalThread::GetFrames() const
{
    return frames_;
}

std::string BacktraceLocalThread::GetFormattedStr(bool withThreadName, bool isJson)
{
    if (frames_.empty()) {
        return "";
    }

    std::ostringstream ss;
    if (withThreadName && (tid_ > 0)) {
        std::string threadName;
        // Tid:1676, Name:IPC_3_1676
        ReadThreadName(tid_, threadName);
        ss << "Tid:" << tid_ << ", Name:" << threadName << std::endl;
    }
    if (isJson) {
#ifndef is_ohos_lite
        ss << DfxJsonFormatter::GetFramesJson(frames_);
#endif
    } else {
        ss << DfxFrameFormatter::GetFramesStr(frames_);
    }
    return ss.str();
}

void BacktraceLocalThread::SetMaxFrameNums(size_t maxFrameNums)
{
    maxFrameNums_ = maxFrameNums;
}
} // namespace HiviewDFX
} // namespace OHOS
