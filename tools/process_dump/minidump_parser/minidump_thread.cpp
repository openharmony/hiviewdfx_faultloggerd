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
#include "minidump_config.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpThread::MinidumpThread(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : memoryReader_(memoryReader),
      memory_(nullptr),
      context_(nullptr),
      isValid_(false)
{
}

bool MinidumpThread::Read()
{
    memory_.reset();
    context_.reset();
    isValid_ = false;

    if (!memoryReader_->ReadBytes(&thread_, sizeof(thread_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_THREAD_READ, "Cannot read thread data", __LINE__);
        DFXLOGE("MinidumpThread cannot read thread");
        return false;
    }

    if (thread_.stack.memory.rva == 0 || thread_.stack.memory.dataSize == 0 ||
        thread_.stack.memory.dataSize > std::numeric_limits<uint64_t>::max() - thread_.stack.startOfMemoryRange) {
        DFXLOGW("MinidumpThread has a memory region problem");
    } else {
        memory_ = std::make_shared<MinidumpMemoryRegion>(memoryReader_);
        memory_->SetDescriptor(thread_.stack);
    }
    isValid_ = true;
    lastError_.Clear();
    return true;
}

std::shared_ptr<MinidumpMemoryRegion> MinidumpThread::GetMemory() const
{
    if (!isValid_) {
        return nullptr;
    }
    return memory_;
}

std::shared_ptr<MinidumpContext> MinidumpThread::GetContext() const
{
    if (!isValid_) {
        return nullptr;
    }

    if (!context_) {
        off_t position = memoryReader_->Tell();
        if (!memoryReader_->SeekSet(thread_.threadContext.rva)) {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "Cannot seek to thread context", __LINE__);
            DFXLOGE("MinidumpThread cannot seek to context");
            return nullptr;
        }

        context_ = std::make_shared<MinidumpContext>(memoryReader_);
        if (!context_->Read(thread_.threadContext.dataSize)) {
            lastError_ = context_->GetLastError();
            DFXLOGE("MinidumpThread cannot read context");
            context_.reset();
            return nullptr;
        }
        memoryReader_->SeekSet(position);
    }
    lastError_.Clear();
    return context_;
}

bool MinidumpThread::GetThreadID(uint32_t& threadId) const
{
    if (!isValid_) {
        return false;
    }
    threadId = thread_.threadId;
    return true;
}

uint64_t MinidumpThread::GetStartOfStackMemoryRange() const
{
    if (!isValid_) return 0;
    return thread_.stack.startOfMemoryRange;
}

void MinidumpThread::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpThread cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpThread");
    DFXLOGI("  threadId             = %{public}d", thread_.threadId);
    DFXLOGI("  suspendCount         = %{public}d", thread_.suspendCount);
    DFXLOGI("  priorityClass        = %{public}d", thread_.priorityClass);
    DFXLOGI("  priority             = %{public}d", thread_.priority);
    DFXLOGI("  teb                  = 0x%{public}" PRIx64, thread_.teb);
    DFXLOGI("  stack.startOfMemoryRange = 0x%{public}" PRIx64, thread_.stack.startOfMemoryRange);
    DFXLOGI("  stack.memory.dataSize = %{public}d", thread_.stack.memory.dataSize);
    DFXLOGI("  stack.memory.rva     = 0x%{public}x", thread_.stack.memory.rva);
    DFXLOGI("  threadContext.dataSize = %{public}d", thread_.threadContext.dataSize);
    DFXLOGI("  threadContext.rva    = 0x%{public}x", thread_.threadContext.rva);
}

MinidumpThreadList::MinidumpThreadList(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader),
      idToThreadMap_(),
      threads_(nullptr),
      threadCount_(0)
{
}

bool MinidumpThreadList::ReadThreadCount(uint32_t expectedSize, uint32_t& threadCount)
{
    isValid_ = false;
    idToThreadMap_.clear();
    threads_.reset();
    threadCount_ = 0;

    if (expectedSize < sizeof(threadCount)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_CORRUPTED_DATA, "Invalid thread list size", __LINE__);
        return false;
    }
    if (!memoryReader_->ReadBytes(&threadCount, sizeof(threadCount))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_THREAD_READ, "Cannot read thread count", __LINE__);
        DFXLOGE("MinidumpThreadList cannot read thread count");
        return false;
    }

    if (threadCount > MinidumpConfigManager::Instance().GetConfig().maxThreads) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_THREAD_COUNT, "Thread count exceeds maximum", __LINE__);
        DFXLOGE("MinidumpThreadList thread count %{public}u exceeds maximum %{public}u",
                threadCount, MinidumpConfigManager::Instance().GetConfig().maxThreads);
        return false;
    }

    if (threadCount == 0) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_THREAD_COUNT, "Thread count is zero", __LINE__);
        return false;
    }
    return true;
}

bool MinidumpThreadList::ReadThreadData(uint32_t threadCount)
{
    threadCount_ = threadCount;
    auto threads = std::make_shared<std::vector<MinidumpThread>>(
        threadCount, MinidumpThread(memoryReader_));

    for (uint32_t i = 0; i < threadCount; ++i) {
        MinidumpThread* thread = &(*threads)[i];
        if (!thread->Read()) {
            lastError_ = thread->GetLastError();
            DFXLOGE("MinidumpThreadList cannot initialize thread %{public}u", i);
            return false;
        }

        uint32_t threadId;
        if (!thread->GetThreadID(threadId)) {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_THREAD_READ, "Cannot get thread ID", __LINE__);
            return false;
        }

        if (idToThreadMap_.find(threadId) != idToThreadMap_.end()) {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_ALREADY_EXISTS, "Duplicate thread ID", __LINE__);
            return false;
        }
        idToThreadMap_[threadId] = std::shared_ptr<MinidumpThread>(threads_, &(*threads)[i]);
    }
    threads_ = std::move(threads);
    return true;
}

bool MinidumpThreadList::Read(uint32_t expectedSize)
{
    uint32_t threadCount = 0;
    if (!ReadThreadCount(expectedSize, threadCount)) {
        return false;
    }
    if (!ReadThreadData(threadCount)) {
        return false;
    }
    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("ThreadList", threadCount);
    }
    return true;
}

std::shared_ptr<MinidumpThread> MinidumpThreadList::GetThreadAtIndex(uint32_t index) const
{
    if (!isValid_ || index >= threadCount_) {
        return nullptr;
    }
    return std::shared_ptr<MinidumpThread>(threads_, const_cast<MinidumpThread*>(&(*threads_)[index]));
}

std::shared_ptr<MinidumpThread> MinidumpThreadList::GetThreadByID(uint32_t threadId)
{
    if (!isValid_) {
        return nullptr;
    }

    IDToThreadMap::iterator it = idToThreadMap_.find(threadId);
    if (it == idToThreadMap_.end()) {
        return nullptr;
    }
    return it->second;
}

void MinidumpThreadList::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpThreadList cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpThreadList");
    DFXLOGI("  thread count = %{public}u", threadCount_);

    for (uint32_t i = 0; i < threadCount_; ++i) {
        DFXLOGI("thread[%{public}u]", i);
        (*threads_)[i].Print();
        DFXLOGI(" ");
    }
    DFXLOGI("\n");
}

}
}
