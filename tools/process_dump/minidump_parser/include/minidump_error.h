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

#ifndef MINIDUMP_ERROR_H
#define MINIDUMP_ERROR_H

#include <cstdint>
#include <string>

namespace OHOS {
namespace HiviewDFX {

enum class MinidumpError : uint32_t {
    SUCCESS                    = 0,
    ERROR_FILE_OPEN            = 1,
    ERROR_FILE_READ            = 2,
    ERROR_FILE_SEEK            = 3,
    ERROR_FILE_SIZE            = 4,
    ERROR_SIGNATURE            = 5,
    ERROR_VERSION              = 6,
    ERROR_ENDIANNESS           = 7,
    ERROR_STREAM_COUNT         = 8,
    ERROR_STREAM_READ          = 9,
    ERROR_STREAM_TYPE          = 10,
    ERROR_MEMORY_ALLOC         = 11,
    ERROR_MEMORY_SIZE          = 12,
    ERROR_MEMORY_OVERFLOW      = 13,
    ERROR_THREAD_COUNT         = 14,
    ERROR_THREAD_READ          = 15,
    ERROR_MODULE_COUNT         = 16,
    ERROR_MODULE_READ          = 17,
    ERROR_CONTEXT_READ         = 18,
    ERROR_CONTEXT_CPU          = 19,
    ERROR_STRING_LENGTH        = 20,
    ERROR_STRING_READ          = 21,
    ERROR_UTF16_CONVERT        = 22,
    ERROR_RANGE_OVERLAP        = 23,
    ERROR_RANGE_RETRIEVE       = 24,
    ERROR_DESCRIPTOR_NULL      = 25,
    ERROR_INVALID_ARGUMENT     = 26,
    ERROR_ALREADY_EXISTS       = 27,
    ERROR_NOT_FOUND            = 28,
    ERROR_CORRUPTED_DATA       = 29,
    ERROR_CHECKSUM_MISMATCH    = 30,
    ERROR_UNKNOWN              = 31,
};

class MinidumpErrorInfo {
public:
    MinidumpErrorInfo() : error_(MinidumpError::SUCCESS), message_(""), line_(0) {}
    
    MinidumpErrorInfo(MinidumpError error, const std::string& message, uint32_t line = 0)
        : error_(error), message_(message), line_(line) {}

    MinidumpError GetError() const { return error_; }
    const std::string& GetMessage() const { return message_; }
    uint32_t GetLine() const { return line_; }
    
    bool IsSuccess() const { return error_ == MinidumpError::SUCCESS; }
    bool IsError() const { return error_ != MinidumpError::SUCCESS; }
    
    std::string ToString() const
    {
        return "Error[" + std::to_string(static_cast<uint32_t>(error_)) + "] at line " +
               std::to_string(line_) + ": " + message_;
    }

    void Clear()
    {
        error_ = MinidumpError::SUCCESS;
        message_.clear();
        line_ = 0;
    }

private:
    MinidumpError error_;
    std::string message_;
    uint32_t line_;
};
}
}

#endif