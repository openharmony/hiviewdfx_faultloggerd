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
MinidumpThreadName::MinidumpThreadName(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : memoryReader_(memoryReader),
      threadNameValid_(false),
      threadName_(),
      name_(nullptr),
      isValid_(false)
{
}

bool MinidumpThreadName::Read()
{
    isValid_ = false;
    threadNameValid_ = false;

    if (!memoryReader_->ReadBytes(&threadName_, sizeof(threadName_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ,
            std::string("Cannot read thread name"), __LINE__);
        DFXLOGE("MinidumpThreadName cannot read thread name");
        return false;
    }
    threadNameValid_ = true;
    return true;
}

bool MinidumpThreadName::ReadAuxiliaryData()
{
    if (!threadNameValid_) {
        return false;
    }
    name_.reset();
    if (threadName_.rvaOfThreadName) {
        name_ = memoryReader_->ReadString(threadName_.rvaOfThreadName);
    }
    if (!name_) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STRING_READ,
            std::string("Cannot read thread name string"), __LINE__);
        DFXLOGE("MinidumpThreadName could not read name");
        return false;
    }
    isValid_ = true;
    lastError_.Clear();
    return true;
}

bool MinidumpThreadName::GetThreadID(uint32_t& threadId) const
{
    if (!isValid_) {
        return false;
    }
    threadId = threadName_.threadId;
    return true;
}

std::string MinidumpThreadName::GetThreadName() const
{
    if (name_) return *name_;
    return "";
}

void MinidumpThreadName::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpThreadName cannot print invalid data");
        return;
    }

    DFXLOGI("MDRawThreadName");
    DFXLOGI("  threadId         = %{public}u", threadName_.threadId);
    DFXLOGI("  threadNameRva    = 0x%{public}" PRIx64, threadName_.rvaOfThreadName);
    if (name_) {
        DFXLOGI("  name             = %{public}s", name_->c_str());
    }
}

MinidumpThreadNameList::MinidumpThreadNameList(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader),
      threadIdToNameMap_(),
      threadNameCount_(0)
{
}

bool MinidumpThreadNameList::Read(uint32_t expectedSize)
{
    threadNameCount_ = 0;
    isValid_ = false;

    uint32_t threadNameCount = 0;
    if (!memoryReader_->ReadBytes(&threadNameCount, sizeof(threadNameCount))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ,
            std::string("Cannot read thread name count"), __LINE__);
        DFXLOGE("MinidumpThreadNameList cannot read thread name count");
        return false;
    }

    if (threadNameCount == 0 || threadNameCount > MinidumpConfigManager::Instance().GetConfig().maxThreads) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_THREAD_COUNT,
            std::string("Invalid thread name count"), __LINE__);
        DFXLOGE("MinidumpThreadNameList count error");
        return false;
    }

    for (uint32_t i = 0; i < threadNameCount; ++i) {
        auto threadName = std::make_shared<MinidumpThreadName>(memoryReader_);

        if (!threadName->Read()) {
            lastError_ = threadName->GetLastError();
            DFXLOGE("MinidumpThreadNameList cannot initialize thread name %{public}u", i);
            return false;
        }
        threadNames_.emplace_back(threadName);
    }

    for (uint32_t i = 0; i < threadNameCount; ++i) {
        auto threadName = threadNames_[i];
        if (!threadName->ReadAuxiliaryData() && !threadName->Valid()) {
            lastError_ = threadName->GetLastError();
            DFXLOGE("MinidumpThreadNameList cannot read thread name %{public}u", i);
            return false;
        }

        uint32_t threadId = 0;
        if (threadName->GetThreadID(threadId)) {
            threadIdToNameMap_.insert(std::make_pair(threadId, threadName->GetThreadName()));
        }
    }
    threadNameCount_ = threadNameCount;
    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("ThreadNameList", threadNameCount);
    }
    return true;
}

std::shared_ptr<MinidumpThreadName> MinidumpThreadNameList::GetThreadNameAtIndex(uint32_t index) const
{
    if (!isValid_ || index >= threadNameCount_) {
        return nullptr;
    }
    return threadNames_[index];
}

std::string MinidumpThreadNameList::GetThreadNameById(uint32_t threadId) const
{
    if (!isValid_) return "";
    auto iter = threadIdToNameMap_.find(threadId);
    std::string threadName;
    if (iter != threadIdToNameMap_.end()) {
        threadName = iter->second;
    }
    return threadName;
}

void MinidumpThreadNameList::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpThreadNameList cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpThreadNameList");
    DFXLOGI("  threadNameCount = %{public}u", threadNameCount_);

    for (uint32_t i = 0; i < threadNameCount_; i++) {
        DFXLOGI("threadName[%{public}u]", i);
        threadNames_[i]->Print();
        DFXLOGI(" ");
    }
    DFXLOGI("\n");
}

}
}