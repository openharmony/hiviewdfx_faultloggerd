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

#include <algorithm>
#include <cinttypes>
#include <securec.h>

#include "dfx_log.h"
#include "minidump_config.h"
#include "minidump_factory.h"
#include "minidump_optimizer.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpModule::MinidumpModule(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : memoryReader_(memoryReader),
      moduleValid_(false),
      module_(),
      name_(nullptr),
      isValid_(false)
{
}

bool MinidumpModule::Read()
{
    moduleValid_ = false;
    isValid_ = false;

    if (!memoryReader_->ReadBytes(&module_, sizeof(module_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MODULE_READ,
            std::string("Cannot read module data"), __LINE__);
        DFXLOGE("MinidumpModule cannot read module");
        return false;
    }
    moduleValid_ = true;
    return true;
}

bool MinidumpModule::ReadAuxiliaryData()
{
    if (!moduleValid_) {
        return false;
    }
    name_.reset();

    if (module_.moduleNameRva > 0) {
        name_ = memoryReader_->ReadString(module_.moduleNameRva);
    }
    if (!name_) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STRING_READ,
            std::string("Cannot read module name"), __LINE__);
        DFXLOGE("MinidumpModule could not read name");
        return false;
    }
    isValid_ = true;
    lastError_.Clear();
    return true;
}

std::string MinidumpModule::CodeFile() const
{
    if (name_) return *name_;
    return "";
}

std::string MinidumpModule::CodeIdentifier() const
{
    char identifier[128] = { 0 };
    if (snprintf_s(identifier, sizeof(identifier), sizeof(identifier) - 1,
        "%08x%x", module_.timeDataStamp, module_.sizeOfImage) <= 0) {
        return "";
    }
    return std::string(identifier);
}

std::string MinidumpModule::DebugFile() const
{
    return "";
}

std::string MinidumpModule::DebugIdentifier() const
{
    char identifier[128];
    if (snprintf_s(identifier, sizeof(identifier), sizeof(identifier) - 1,
        "%08x%08x", module_.timeDataStamp, module_.checksum) <= 0) {
        return "";
    }
    return std::string(identifier);
}

std::string MinidumpModule::Version() const
{
    return "";
}

void MinidumpModule::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpModule cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpModule");
    DFXLOGI("  baseOfImage    = 0x%{public}" PRIx64, module_.baseOfImage);
    DFXLOGI("  sizeOfImage    = %{public}u", module_.sizeOfImage);
    DFXLOGI("  checksum       = 0x%{public}x", module_.checksum);
    DFXLOGI("  timeDataStamp  = 0x%{public}x", module_.timeDataStamp);
    DFXLOGI("  moduleNameRva  = 0x%{public}x", module_.moduleNameRva);
    if (name_) {
        DFXLOGI("  name           = %{public}s", name_->c_str());
    }
}

MinidumpModuleList::MinidumpModuleList(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader),
      moduleCount_(0)
{
}

bool MinidumpModuleList::ReadModuleRawData(uint32_t moduleCount)
{
    for (uint32_t i = 0; i < moduleCount; ++i) {
        auto minidumpModule = std::make_shared<MinidumpModule>(memoryReader_);
        if (!minidumpModule->Read()) {
            lastError_ = minidumpModule->GetLastError();
            DFXLOGE("MinidumpModuleList cannot initialize module %{public}u", i);
            return false;
        }
        modules_.emplace_back(std::move(minidumpModule));
    }
    return true;
}

bool MinidumpModuleList::ReadModuleAuxiliaryDataAndIndex(uint32_t moduleCount)
{
    for (uint32_t i = 0; i < moduleCount; ++i) {
        auto minidumpModule = modules_[i];
        if (!minidumpModule->ReadAuxiliaryData() && !minidumpModule->Valid()) {
            lastError_ = minidumpModule->GetLastError();
            DFXLOGE("MinidumpModuleList cannot read auxiliary data for module %{public}u", i);
            return false;
        }

        uint64_t baseAddress = minidumpModule->BaseAddress();
        uint64_t moduleSize = minidumpModule->Size();
        if (baseAddress == static_cast<uint64_t>(-1)) {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_CORRUPTED_DATA,
                std::string("Invalid module base address"), __LINE__);
            DFXLOGE("MinidumpModuleList found bad base address for module %{public}u", i);
            return false;
        }
        if (PerformanceOptimizer::Instance().GetConfig().enableRangeMap) {
            if (!PerformanceOptimizer::Instance().GetModuleRangeMap().StoreRange(baseAddress, moduleSize, i)) {
                lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_RANGE_OVERLAP,
                std::string("Module range overlap detected"), __LINE__);
                DFXLOGW("MinidumpModuleList detected range overlap for module %{public}u", i);
            }
        }
        if (PerformanceOptimizer::Instance().GetConfig().enableIntervalTree) {
            auto& moduleTree = PerformanceOptimizer::Instance().GetModuleIntervalTree();
            if (!moduleTree.Insert(baseAddress, baseAddress + moduleSize, i)) {
                lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_RANGE_OVERLAP,
                    std::string("Module range overlap detected"), __LINE__);
                DFXLOGW("MinidumpModuleList detected range overlap for module %{public}u", i);
            }
        }
        if (PerformanceOptimizer::Instance().GetConfig().enableBitmapIndex) {
            auto& bitmapIndex = PerformanceOptimizer::Instance().GetBitmapIndex();
            bitmapIndex.MarkRange(baseAddress, baseAddress + moduleSize);
        }
    }
    return true;
}

bool MinidumpModuleList::Read(uint32_t expectedSize)
{
    moduleCount_ = 0;
    isValid_ = false;

    uint32_t moduleCount = 0;
    if (!memoryReader_->ReadBytes(&moduleCount, sizeof(moduleCount))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MODULE_READ,
            std::string("Cannot read module count"), __LINE__);
        DFXLOGE("MinidumpModuleList cannot read module count");
        return false;
    }

    if (moduleCount == 0 || moduleCount > MinidumpConfigManager::Instance().GetConfig().maxModules) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MODULE_COUNT,
            std::string("Invalid module count"), __LINE__);
        DFXLOGE("MinidumpModuleList module count %{public}u exceeds maximum %{public}u",
                moduleCount, MinidumpConfigManager::Instance().GetConfig().maxModules);
        return false;
    }

    if (!ReadModuleRawData(moduleCount)) {
        return false;
    }
    if (!ReadModuleAuxiliaryDataAndIndex(moduleCount)) {
        return false;
    }
    moduleCount_ = moduleCount;
    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("ModuleList", moduleCount);
    }
    return true;
}

std::shared_ptr<MinidumpModule> MinidumpModuleList::GetModuleForAddress(uint64_t address)
{
    if (!isValid_) {
        return nullptr;
    }

    uint32_t moduleIndex = 0;
    bool found = false;
    if (PerformanceOptimizer::Instance().GetConfig().enableRangeMap) {
        found = PerformanceOptimizer::Instance().GetModuleRangeMap().RetrieveRange(address, &moduleIndex);
    }
    if (PerformanceOptimizer::Instance().GetConfig().enableBitmapIndex) {
        auto& bitmapIndex = PerformanceOptimizer::Instance().GetBitmapIndex();
        if (!bitmapIndex.IsInRange(address) && bitmapIndex.Size() != 0) {
            return nullptr;
        }
    }
    if (PerformanceOptimizer::Instance().GetConfig().enableIntervalTree) {
        auto& moduleTree = PerformanceOptimizer::Instance().GetModuleIntervalTree();
        found = moduleTree.Search(address, &moduleIndex);
    }
    if (!found) {
        return nullptr;
    }
    return GetModuleAtIndex(moduleIndex);
}

std::shared_ptr<MinidumpModule> MinidumpModuleList::GetModuleAtIndex(uint32_t index) const
{
    if (!isValid_ || index >= moduleCount_) {
        return nullptr;
    }
    return modules_[index];
}

std::shared_ptr<MinidumpModule> MinidumpModuleList::GetMainModule() const
{
    if (!isValid_ || moduleCount_ == 0) {
        return nullptr;
    }
    return GetModuleAtIndex(0);
}

void MinidumpModuleList::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpModuleList cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpModuleList");
    DFXLOGI("  module count = %{public}u", moduleCount_);
    for (uint32_t i = 0; i < moduleCount_; i++) {
        DFXLOGI("module[%{public}u]", i);
        modules_[i]->Print();
        DFXLOGI(" ");
    }
    DFXLOGI("\n");
}

}
}