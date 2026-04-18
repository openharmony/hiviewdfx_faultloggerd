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
    MinidumpMemoryReader(std::shared_ptr<std::istream> stream) { stream_ = stream; }
    bool ReadBytes(void* bytes, size_t count);
    bool SeekSet(off_t offset);
    off_t Tell();

    std::shared_ptr<std::string> ReadString(off_t offset);
    bool ReadUTF8String(off_t offset, std::string* utf8Str);
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
private:
    std::string ConvertUTF16ToUTF8(const std::vector<uint16_t>& in);
    std::shared_ptr<std::istream> stream_;
    MinidumpErrorInfo lastError_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif