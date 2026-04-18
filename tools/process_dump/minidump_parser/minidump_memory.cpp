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

#include "dfx_log.h"
#include "minidump_config.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_optimizer.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpMemoryRegion::MinidumpMemoryRegion(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : memoryReader_(memoryReader),
      memory_(nullptr),
      isValid_(false)
{
}

void MinidumpMemoryRegion::SetDescriptor(MDMemoryDescriptor descriptor)
{
    descriptor_ = descriptor;
    isValid_ = descriptor_.memory.dataSize <=
                 std::numeric_limits<uint64_t>::max() - descriptor_.startOfMemoryRange;
}

std::shared_ptr<std::vector<uint8_t>> MinidumpMemoryRegion::GetMemory()
{
    if (!isValid_) {
        return nullptr;
    }
    if (memory_ != nullptr) {
        return memory_;
    }
    if (descriptor_.memory.dataSize > MinidumpConfigManager::Instance().GetConfig().maxMemoryBytes) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MEMORY_SIZE,
            "Memory region size exceeds maximum", __LINE__);
        DFXLOGE("MinidumpMemoryRegion size %{public}u exceeds maximum %{public}u",
                descriptor_.memory.dataSize, MinidumpConfigManager::Instance().GetConfig().maxMemoryBytes);
        return nullptr;
    }
    auto memory = std::make_shared<std::vector<uint8_t>>(descriptor_.memory.dataSize);

    off_t position = memoryReader_->Tell();
    if (!memoryReader_->SeekSet(descriptor_.memory.rva)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "Cannot seek to memory region", __LINE__);
        DFXLOGE("MinidumpMemoryRegion cannot seek to memory");
        return nullptr;
    }

    if (!memoryReader_->ReadBytes(memory->data(), descriptor_.memory.dataSize)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MEMORY_ALLOC, "Cannot read memory region", __LINE__);
        DFXLOGE("MinidumpMemoryRegion cannot read memory");
        memoryReader_->SeekSet(position);
        return nullptr;
    }
    memory_ = std::move(memory);
    memoryReader_->SeekSet(position);
    
    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    stats.IncrementRead(descriptor_.memory.dataSize);
    lastError_.Clear();
    return memory_;
}

uint64_t MinidumpMemoryRegion::GetBase() const
{
    if (!isValid_) {
        return static_cast<uint64_t>(-1);
    }
    return descriptor_.startOfMemoryRange;
}

uint32_t MinidumpMemoryRegion::GetSize() const
{
    if (!isValid_) {
        return 0;
    }
    return descriptor_.memory.dataSize;
}

void MinidumpMemoryRegion::FreeMemory()
{
    memory_.reset();
}

template<typename T>
bool MinidumpMemoryRegion::GetMemoryAtAddressInternal(uint64_t address, T& value) const
{
    if (memory_ == nullptr) {
        DFXLOGE("MinidumpMemoryRegion invalid data");
        return false;
    }
    value = 0;
    uint64_t base = GetBase();
    if (address < base) {
        DFXLOGE("MinidumpMemoryRegion address below base");
        return false;
    }
    uint64_t offset = address - base;

    if (offset + sizeof(T) > memory_->size()) {
        DFXLOGE("MinidumpMemoryRegion greater size");
        return false;
    }

    value = *reinterpret_cast<const T*>(memory_->data() + offset);
    return true;
}

bool MinidumpMemoryRegion::GetMemoryAtAddress(uint64_t address, uint8_t& value) const
{
    return GetMemoryAtAddressInternal(address, value);
}

bool MinidumpMemoryRegion::GetMemoryAtAddress(uint64_t address, uint16_t& value) const
{
    return GetMemoryAtAddressInternal(address, value);
}

bool MinidumpMemoryRegion::GetMemoryAtAddress(uint64_t address, uint32_t& value) const
{
    return GetMemoryAtAddressInternal(address, value);
}

bool MinidumpMemoryRegion::GetMemoryAtAddress(uint64_t address, uint64_t& value) const
{
    return GetMemoryAtAddressInternal(address, value);
}

void MinidumpMemoryRegion::Print() const
{
    if (!isValid_) {
        DFXLOGE("MinidumpMemoryRegion cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpMemoryRegion");
    DFXLOGI("  base   = 0x%{public}" PRIx64, GetBase());
    DFXLOGI("  size   = %{public}u", GetSize());
}

MinidumpMemoryList::MinidumpMemoryList(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader)
{
}

bool MinidumpMemoryList::ReadRegionCountAndDescriptors(std::vector<MDMemoryDescriptor>& descriptors)
{
    regionCount_ = 0;
    if (!memoryReader_->ReadBytes(&regionCount_, sizeof(regionCount_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MEMORY_ALLOC, "Cannot read memory region count", __LINE__);
        DFXLOGE("MinidumpMemoryList cannot read region count");
        return false;
    }
    if (regionCount_ > MinidumpConfigManager::Instance().GetConfig().maxMemoryRegions) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MEMORY_SIZE,
            "Memory region count exceeds maximum", __LINE__);
        DFXLOGE("MinidumpMemoryList region count %{public}u exceeds maximum %{public}u",
                regionCount_, MinidumpConfigManager::Instance().GetConfig().maxMemoryRegions);
        return false;
    }
    if (regionCount_ == 0) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MEMORY_SIZE, "Memory region count is zero", __LINE__);
        return false;
    }
    descriptors.resize(regionCount_);
    if (!memoryReader_->ReadBytes(descriptors.data(), sizeof(MDMemoryDescriptor) * regionCount_)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_MEMORY_ALLOC,
            "Cannot read memory region descriptors", __LINE__);
        DFXLOGE("MinidumpMemoryList cannot read memory region list");
        return false;
    }
    return true;
}

bool MinidumpMemoryList::BuildMemoryRegions(const std::vector<MDMemoryDescriptor>& descriptors)
{
    regions_.reserve(regionCount_);
    for (uint32_t i = 0; i < descriptors.size(); ++i) {
        if (PerformanceOptimizer::Instance().GetConfig().enableRangeMap) {
            PerformanceOptimizer::Instance().GetMemoryRangeMap().StoreRange(descriptors[i].startOfMemoryRange,
                descriptors[i].memory.dataSize, i);
        }
        if (PerformanceOptimizer::Instance().GetConfig().enableIntervalTree) {
            auto& memoryTree = PerformanceOptimizer::Instance().GetMemoryIntervalTree();
            if (!memoryTree.Insert(descriptors[i].startOfMemoryRange,
                descriptors[i].startOfMemoryRange + descriptors[i].memory.dataSize, i)) {
                lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_RANGE_OVERLAP,
                    std::string("memory range overlap detected"), __LINE__);
                DFXLOGW("MinidumpMemoryList detected range overlap for memory %{public}u", i);
            }
        }
        if (PerformanceOptimizer::Instance().GetConfig().enableBitmapIndex) {
            auto& bitmapIndex = PerformanceOptimizer::Instance().GetBitmapIndex();
            bitmapIndex.MarkRange(descriptors[i].startOfMemoryRange,
                descriptors[i].startOfMemoryRange + descriptors[i].memory.dataSize);
        }
        auto region = std::make_shared<MinidumpMemoryRegion>(memoryReader_);
        region->SetDescriptor(descriptors[i]);
        regions_.emplace_back(std::move(region));
    }
    descriptors_ = descriptors;
    return true;
}

bool MinidumpMemoryList::Read(uint32_t expectedSize)
{
    std::vector<MDMemoryDescriptor> descriptors;
    if (!ReadRegionCountAndDescriptors(descriptors)) {
        return false;
    }
    if (!BuildMemoryRegions(descriptors)) {
        return false;
    }
    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("MemoryList", regionCount_);
    }
    return true;
}

std::shared_ptr<MinidumpMemoryRegion> MinidumpMemoryList::GetMemoryRegionAtIndex(uint32_t index)
{
    if (!isValid_ || index >= regionCount_) {
        return nullptr;
    }
    return regions_[index];
}

std::shared_ptr<MinidumpMemoryRegion> MinidumpMemoryList::GetMemoryRegionForAddress(uint64_t address)
{
    if (!isValid_) {
        return nullptr;
    }

    uint32_t regionIndex = 0;
    bool found = false;
    if (PerformanceOptimizer::Instance().GetConfig().enableRangeMap) {
        found = PerformanceOptimizer::Instance().GetMemoryRangeMap().RetrieveRange(address, &regionIndex);
    }
    if (PerformanceOptimizer::Instance().GetConfig().enableBitmapIndex) {
        auto& bitmapIndex = PerformanceOptimizer::Instance().GetBitmapIndex();
        if (!bitmapIndex.IsInRange(address) && bitmapIndex.Size() != 0) {
            return nullptr;
        }
    }
    if (PerformanceOptimizer::Instance().GetConfig().enableIntervalTree) {
        auto& memoryTree = PerformanceOptimizer::Instance().GetMemoryIntervalTree();
        found = memoryTree.Search(address, &regionIndex);
    }
    if (!found) {
        return nullptr;
    }
    return GetMemoryRegionAtIndex(regionIndex);
}

void MinidumpMemoryList::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpMemoryList cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpMemoryList");
    DFXLOGI("  region count = %{public}u", regionCount_);

    for (uint32_t i = 0; i < regionCount_; i++) {
        DFXLOGI("region[%{public}u]", i);
        DFXLOGI("  base   = 0x%{public}" PRIx64, descriptors_[i].startOfMemoryRange);
        DFXLOGI("  size   = %{public}u", descriptors_[i].memory.dataSize);
        DFXLOGI("  rva    = 0x%{public}x", descriptors_[i].memory.rva);
        DFXLOGI(" ");
    }
    DFXLOGI("\n");
}

}
}