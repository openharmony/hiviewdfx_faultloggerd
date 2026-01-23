/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef MEMORYREADER_H
#define MEMORYREADER_H

#include "smart_fd.h"
namespace OHOS {
namespace HiviewDFX {
class MemoryReader {
public:
    virtual ~MemoryReader() = default;
    virtual bool ReadMemory(const uintptr_t src, void* dst, size_t dataSize) = 0;
};

class ThreadMemoryReader : public MemoryReader {
public:
    ThreadMemoryReader(uintptr_t stackBegin, uintptr_t stackEnd);
    ~ThreadMemoryReader() override = default;
    bool ReadMemory(const uintptr_t src, void* dst, size_t dataSize) override;
private:
    uintptr_t stackBegin_{0};
    uintptr_t stackEnd_{0};
};

class ProcessMemoryReader : public MemoryReader {
public:
    ProcessMemoryReader();
    ~ProcessMemoryReader() override = default;
    bool ReadMemory(const uintptr_t src, void* dst, size_t dataSize) override;
private:
    bool ReadCharByPipe(uintptr_t src, void* dst) const;
    SmartFd writeFd_;
    SmartFd readFd_;
    uintptr_t readablePageMask_ = 0;
    uintptr_t readablePage_ = static_cast<uintptr_t>(-1);
};
}
}
#endif