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

#ifndef MINIDUMP_INDEX_H
#define MINIDUMP_INDEX_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "minidump_optimizer.h"

namespace OHOS {
namespace HiviewDFX {

class IAddressIndex {
public:
    virtual ~IAddressIndex() = default;

    virtual bool Insert(uint64_t start, uint64_t end, uint32_t value) = 0;
    virtual bool Lookup(uint64_t address, uint32_t& result) const = 0;
    virtual bool IsInRange(uint64_t address) const = 0;
    virtual void Clear() = 0;
    virtual size_t Size() const = 0;
    virtual const char* Name() const = 0;
};

class LinearScanIndex : public IAddressIndex {
public:
    bool Insert(uint64_t start, uint64_t end, uint32_t value) override;
    bool Lookup(uint64_t address, uint32_t& result) const override;
    bool IsInRange(uint64_t address) const override;
    void Clear() override;
    size_t Size() const override { return entries_.size(); }
    const char* Name() const override { return "LinearScan"; }

private:
    struct Entry {
        uint64_t start;
        uint64_t end;
        uint32_t value;
    };
    std::vector<Entry> entries_;
};

class IntervalTreeIndex : public IAddressIndex {
public:
    IntervalTreeIndex() : tree_() {}

    bool Insert(uint64_t start, uint64_t end, uint32_t value) override
    {
        return tree_.Insert(start, end, value);
    }
    bool Lookup(uint64_t address, uint32_t& result) const override
    {
        return tree_.Search(address, &result);
    }
    bool IsInRange(uint64_t address) const override
    {
        uint32_t tmp;
        return tree_.Search(address, &tmp);
    }
    void Clear() override { tree_.Clear(); }
    size_t Size() const override { return tree_.Size(); }
    const char* Name() const override { return "IntervalTree"; }

private:
    IntervalTree<uint64_t, uint32_t> tree_;
};

class RangeMapIndex : public IAddressIndex {
public:
    RangeMapIndex() : map_() {}

    bool Insert(uint64_t start, uint64_t end, uint32_t value) override;
    bool Lookup(uint64_t address, uint32_t& result) const override;
    bool IsInRange(uint64_t address) const override;
    void Clear() override { map_.Clear(); }
    size_t Size() const override { return map_.GetRangeSize(); }
    const char* Name() const override { return "RangeMap"; }

private:
    RangeMap<uint64_t, uint32_t> map_;
};

class BitmapFilterIndex : public IAddressIndex {
public:
    BitmapFilterIndex(uint64_t addressRange, uint32_t granularity,
                      std::shared_ptr<IAddressIndex> preciseIndex);
    BitmapFilterIndex();

    bool Insert(uint64_t start, uint64_t end, uint32_t value) override;
    bool Lookup(uint64_t address, uint32_t& result) const override;
    bool IsInRange(uint64_t address) const override;
    void Clear() override;
    size_t Size() const override;
    const char* Name() const override { return "BitmapFilter"; }

    void SetGranularity(uint32_t granularity);
    size_t MarkedCount() const { return bitmap_.MarkedCount(); }

private:
    BitmapIndex bitmap_;
    std::shared_ptr<IAddressIndex> preciseIndex_;
};

class AdaptiveAddressIndex : public IAddressIndex {
public:
    static constexpr uint64_t DEFAULT_ADDRESS_RANGE = 0x800000000000ULL;
    static constexpr uint32_t DEFAULT_BITMAP_GRANULARITY = 134217728;
    static constexpr size_t LINEAR_THRESHOLD = 64;
    static constexpr size_t BITMAP_THRESHOLD = 256;

    AdaptiveAddressIndex(uint64_t addressRange = DEFAULT_ADDRESS_RANGE,
                         uint32_t granularity = DEFAULT_BITMAP_GRANULARITY);
    ~AdaptiveAddressIndex() = default;

    bool Insert(uint64_t start, uint64_t end, uint32_t value) override;
    bool Lookup(uint64_t address, uint32_t& result) const override;
    bool IsInRange(uint64_t address) const override;
    void Clear() override;
    size_t Size() const override { return masterList_.size(); }
    const char* Name() const override { return current_ ? current_->Name() : "Adaptive(None)"; }

    size_t GetMigrationCount() const { return migrationCount_; }

private:
    struct IndexEntry {
        uint64_t start;
        uint64_t end;
        uint32_t value;
    };

    void EnsureStrategy();
    void RebuildCurrentIndex();

    uint64_t addressRange_;
    uint32_t granularity_;
    size_t migrationCount_;
    std::vector<IndexEntry> masterList_;
    std::shared_ptr<IAddressIndex> current_;
    std::shared_ptr<IAddressIndex> linearIndex_;
    std::shared_ptr<IntervalTreeIndex> treeIndex_;
    std::shared_ptr<BitmapFilterIndex> bitmapIndex_;
};

}
}

#endif
