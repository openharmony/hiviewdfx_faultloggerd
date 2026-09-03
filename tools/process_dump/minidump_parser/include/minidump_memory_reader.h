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

#ifndef MEMORY_READER_H
#define MEMORY_READER_H
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "minidump_error.h"
namespace OHOS {
namespace HiviewDFX {

class MinidumpMemoryReader {
public:
    explicit MinidumpMemoryReader(std::shared_ptr<std::istream> stream);
    explicit MinidumpMemoryReader(const std::string& path);
    ~MinidumpMemoryReader();

    MinidumpMemoryReader(const MinidumpMemoryReader&) = delete;
    MinidumpMemoryReader& operator=(const MinidumpMemoryReader&) = delete;

    bool ReadBytes(void* bytes, size_t count);
    bool SeekSet(off_t offset);
    off_t Tell();
    bool ValidateStreamExtent(uint32_t rva, uint32_t dataSize);

    std::shared_ptr<std::string> ReadString(off_t offset);
    bool ReadUTF8String(off_t offset, std::string* utf8Str);
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }

    bool IsMmapEnabled() const { return mmapData_ != nullptr; }
    size_t GetFileSize() const { return fileSize_; }

private:
    enum class BackendType { ISTREAM, MMAP };
    BackendType backend_ = BackendType::ISTREAM;

    bool InitMmap(const std::string& path);
    bool ReadFromStream(void* bytes, size_t count);
    bool ReadFromMmap(void* bytes, size_t count);
    void InitStreamSize();

    std::string ConvertUTF16ToUTF8(const std::vector<uint16_t>& in);
    std::shared_ptr<std::istream> stream_;
    MinidumpErrorInfo lastError_;

    uint8_t* mmapData_ = nullptr;
    size_t mmapSize_ = 0;
    off_t mmapPos_ = 0;
    int mmapFd_ = -1;
    size_t fileSize_ = 0;
    char* streamBuf_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif