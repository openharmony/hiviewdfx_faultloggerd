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

#include "minidump_index.h"

#include <algorithm>
#include <cstring>
#include <cinttypes>

#include "dfx_log.h"

namespace {
constexpr uint64_t DEFAULT_ADDRESS_RANGE = 0x800000000000ULL;
constexpr uint32_t DEFAULT_BITMAP_GRANULARITY = 134217728;
}

namespace OHOS {
namespace HiviewDFX {

bool LinearScanIndex::Insert(uint64_t start, uint64_t end, uint32_t value)
{
    if (start > end) {
        return false;
    }
    for (const auto& entry : entries_) {
        if (start <= entry.end && end >= entry.start) {
            return false;
        }
    }
    entries_.push_back({start, end, value});
    return true;
}

bool LinearScanIndex::Lookup(uint64_t address, uint32_t& result) const
{
    for (const auto& entry : entries_) {
        if (address >= entry.start && address <= entry.end) {
            result = entry.value;
            return true;
        }
    }
    return false;
}

bool LinearScanIndex::IsInRange(uint64_t address) const
{
    for (const auto& entry : entries_) {
        if (address >= entry.start && address <= entry.end) {
            return true;
        }
    }
    return false;
}

void LinearScanIndex::Clear()
{
    entries_.clear();
}

bool RangeMapIndex::Insert(uint64_t start, uint64_t end, uint32_t value)
{
    if (start > end) {
        return false;
    }
    uint64_t size = end - start + 1;
    return map_.StoreRange(start, size, value);
}

bool RangeMapIndex::Lookup(uint64_t address, uint32_t& result) const
{
    return map_.RetrieveRange(address, &result);
}

bool RangeMapIndex::IsInRange(uint64_t address) const
{
    return map_.HasRange(address);
}

BitmapFilterIndex::BitmapFilterIndex(uint64_t addressRange, uint32_t granularity,
                                     std::shared_ptr<IAddressIndex> preciseIndex)
    : bitmap_(addressRange, granularity), preciseIndex_(preciseIndex)
{
}

BitmapFilterIndex::BitmapFilterIndex()
    : bitmap_(DEFAULT_ADDRESS_RANGE, DEFAULT_BITMAP_GRANULARITY),
      preciseIndex_(std::make_shared<IntervalTreeIndex>())
{
}

bool BitmapFilterIndex::Insert(uint64_t start, uint64_t end, uint32_t value)
{
    bitmap_.MarkRange(start, end);
    if (preciseIndex_) {
        return preciseIndex_->Insert(start, end, value);
    }
    return true;
}

bool BitmapFilterIndex::Lookup(uint64_t address, uint32_t& result) const
{
    if (bitmap_.Size() != 0 && !bitmap_.IsInRange(address)) {
        return false;
    }
    if (preciseIndex_) {
        return preciseIndex_->Lookup(address, result);
    }
    return false;
}

bool BitmapFilterIndex::IsInRange(uint64_t address) const
{
    if (bitmap_.Size() != 0 && !bitmap_.IsInRange(address)) {
        return false;
    }
    if (preciseIndex_) {
        return preciseIndex_->IsInRange(address);
    }
    return false;
}

void BitmapFilterIndex::Clear()
{
    bitmap_.Clear();
    if (preciseIndex_) {
        preciseIndex_->Clear();
    }
}

size_t BitmapFilterIndex::Size() const
{
    return preciseIndex_ ? preciseIndex_->Size() : 0;
}

void BitmapFilterIndex::SetGranularity(uint32_t granularity)
{
    bitmap_ = BitmapIndex(0x800000000000ULL, granularity);
}

AdaptiveAddressIndex::AdaptiveAddressIndex(uint64_t addressRange, uint32_t granularity)
    : addressRange_(addressRange),
      granularity_(granularity),
      migrationCount_(0)
{
    linearIndex_ = std::make_shared<LinearScanIndex>();
    current_ = linearIndex_;
}

void AdaptiveAddressIndex::EnsureStrategy()
{
    size_t entryCount = masterList_.size();
    std::shared_ptr<IAddressIndex> target;

    if (entryCount <= LINEAR_THRESHOLD) {
        target = linearIndex_;
    } else if (entryCount <= BITMAP_THRESHOLD) {
        if (!treeIndex_) {
            treeIndex_ = std::make_shared<IntervalTreeIndex>();
        }
        target = treeIndex_;
    } else {
        if (!bitmapIndex_) {
            auto tree = treeIndex_ ? treeIndex_ : std::make_shared<IntervalTreeIndex>();
            bitmapIndex_ = std::make_shared<BitmapFilterIndex>(addressRange_, granularity_, tree);
        }
        target = bitmapIndex_;
    }

    if (target.get() != current_.get()) {
        current_ = target;
        RebuildCurrentIndex();
        migrationCount_++;
        DFXLOGI("AdaptiveAddressIndex migrated to %{public}s at size=%{public}zu",
                current_->Name(), entryCount);
    }
}

void AdaptiveAddressIndex::RebuildCurrentIndex()
{
    if (!current_) {
        return;
    }
    current_->Clear();
    for (const auto& entry : masterList_) {
        current_->Insert(entry.start, entry.end, entry.value);
    }
}

bool AdaptiveAddressIndex::Insert(uint64_t start, uint64_t end, uint32_t value)
{
    if (start > end) {
        return false;
    }
    masterList_.push_back({start, end, value});
    bool inserted = true;
    if (current_) {
        inserted = current_->Insert(start, end, value);
        if (!inserted) {
            DFXLOGW("AdaptiveAddressIndex: overlap detected for [%{public}" PRIu64 ", %{public}" PRIu64 "]",
                start, end);
        }
    }
    EnsureStrategy();
    return inserted;
}

bool AdaptiveAddressIndex::Lookup(uint64_t address, uint32_t& result) const
{
    if (!current_) {
        return false;
    }
    return current_->Lookup(address, result);
}

bool AdaptiveAddressIndex::IsInRange(uint64_t address) const
{
    if (!current_) {
        return false;
    }
    return current_->IsInRange(address);
}

void AdaptiveAddressIndex::Clear()
{
    masterList_.clear();
    if (linearIndex_) linearIndex_->Clear();
    if (treeIndex_) treeIndex_->Clear();
    if (bitmapIndex_) bitmapIndex_->Clear();
    migrationCount_ = 0;
    current_ = linearIndex_;
}

}
}
