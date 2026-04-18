/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <cinttypes>

#include "dfx_log.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpException::MinidumpException(std::shared_ptr<MinidumpMemoryReader> memoryReader) : MinidumpStream(memoryReader)
{
}

bool MinidumpException::Read(uint32_t expectedSize)
{
    context_.reset();
    isValid_ = false;
    if (expectedSize < sizeof(MDExceptionStream)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ, "Exception stream too small", __LINE__);
        DFXLOGE("MinidumpException expected size %{public}u too small", expectedSize);
        return false;
    }
    if (!memoryReader_->ReadBytes(&exception_, sizeof(exception_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ,
            std::string("Cannot read exception stream"), __LINE__);
        DFXLOGE("MinidumpException cannot read exception stream");
        return false;
    }

    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("Exception", 1);
    }
    return true;
}

bool MinidumpException::GetThreadID(uint32_t& threadId) const
{
    if (!isValid_) {
        return false;
    }
    threadId = exception_.threadId;
    return true;
}

std::shared_ptr<MinidumpContext> MinidumpException::GetContext()
{
    if (!isValid_) {
        return nullptr;
    }

    if (!context_) {
        off_t position = memoryReader_->Tell();

        if (!memoryReader_->SeekSet(exception_.threadContext.rva)) {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK,
                std::string("Cannot seek to exception context"), __LINE__);
            DFXLOGE("MinidumpException cannot seek to context");
            return nullptr;
        }

        context_ = std::make_shared<MinidumpContext>(memoryReader_);
        if (!context_->Read(exception_.threadContext.dataSize)) {
            lastError_ = context_->GetLastError();
            DFXLOGE("MinidumpException cannot read context");
            context_.reset();
            return nullptr;
        }
        memoryReader_->SeekSet(position);
    }
    lastError_.Clear();
    return context_;
}

void MinidumpException::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpException cannot print invalid data");
        return;
    }

    DFXLOGI("MDExceptionStream");
    DFXLOGI("  threadId                 = %{public}u", exception_.threadId);
    DFXLOGI("  exceptionRecord.exceptionCode = %{public}u", exception_.exceptionRecord.exceptionCode);
    DFXLOGI("  exceptionRecord.exceptionFlags = %{public}u", exception_.exceptionRecord.exceptionFlags);
    DFXLOGI("  exceptionRecord.exceptionAddress = 0x%{public}" PRIx64, exception_.exceptionRecord.exceptionAddress);
    auto context = GetContext();
    if (context) {
        context->Print();
    }
    DFXLOGI("\n");
}

}
}