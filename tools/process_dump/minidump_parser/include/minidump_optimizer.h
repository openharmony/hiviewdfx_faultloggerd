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

#ifndef MINIDUMP_ALGORITHM_H
#define MINIDUMP_ALGORITHM_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
template <typename AddressType, typename EntryType>
class RangeMap {
public:
    RangeMap();

    bool StoreRange(AddressType base, AddressType size, EntryType value);

    bool RetrieveRange(AddressType address, EntryType* value) const;

    bool HasRange(AddressType address) const;

    void Clear() noexcept;

    AddressType GetHighestAddress() const noexcept;
    size_t GetRangeSize() const noexcept;
    bool IsEmpty() const noexcept;

private:
    struct RangeMapEntry {
        AddressType base;
        AddressType high;
        EntryType   value;
    };

    typedef std::map<AddressType, RangeMapEntry> AddressToRangeMap;
    typedef typename AddressToRangeMap::const_iterator MapConstIterator;
    typedef typename AddressToRangeMap::value_type MapValue;
    AddressToRangeMap map_;

    AddressType high_;
};

template<typename AddressType, typename EntryType>
struct IntervalNode;

template<typename AddressType, typename EntryType>
class IntervalTree {
public:
    IntervalTree();
    ~IntervalTree();

    bool Insert(AddressType low, AddressType high, EntryType value);

    bool Search(AddressType address, EntryType* result) const;

    std::vector<EntryType> SearchRange(AddressType low, AddressType high) const;

    void Clear();

    size_t Size() const;
    bool Empty() const;
    AddressType GetMaxAddress() const;

private:
    IntervalNode<AddressType, EntryType>* root_;
    size_t size_;

    int GetHeight(IntervalNode<AddressType, EntryType>* node) const;

    AddressType GetMax(IntervalNode<AddressType, EntryType>* node) const;

    int GetBalance(IntervalNode<AddressType, EntryType>* node) const;

    IntervalNode<AddressType, EntryType>* RotateRight(IntervalNode<AddressType, EntryType>* y);

    IntervalNode<AddressType, EntryType>* RotateLeft(IntervalNode<AddressType, EntryType>* x);

    IntervalNode<AddressType, EntryType>* InsertNode(IntervalNode<AddressType, EntryType>* node,
                                                     AddressType low, AddressType high, EntryType value);

    bool SearchNode(IntervalNode<AddressType, EntryType>* node, AddressType address,
                    EntryType* result) const;

    void SearchRangeNode(IntervalNode<AddressType, EntryType>* node, AddressType low,
                         AddressType high, std::vector<EntryType>& results) const;

    void ClearNode(IntervalNode<AddressType, EntryType>* node);
};

class BitmapIndex {
public:
    // 0x800000000000ULL : 128TB
    // 134217728: 128 MB
    explicit BitmapIndex(uint64_t addressRange = 0x800000000000ULL, uint32_t granularity = 134217728);
    void MarkRange(uint64_t start, uint64_t end);
    bool IsInRange(uint64_t address) const;
    uint64_t FindNextInRange(uint64_t start) const;
    void Clear();
    size_t Size() const;
    size_t MarkedCount() const;

private:
    uint32_t granularity_;
    std::vector<bool> bitmap_;
};

class PerformanceOptimizer {
public:
    struct Config {
        bool enableRangeMap;
        bool enableIntervalTree;
        bool enableBitmapIndex;
        uint32_t bitmapGranularity;
    };

    struct Statistics {
        size_t intervalTreeModuleSize;
        size_t intervalTreeMemorySize;
        size_t bitmapMarkedCount;
        size_t rangeMapModuleSize;
        size_t rangeMapMemorySize;
    };
    static PerformanceOptimizer& Instance();
    PerformanceOptimizer(const PerformanceOptimizer&) = delete;
    PerformanceOptimizer& operator=(const PerformanceOptimizer&) = delete;
    
    void SetConfig(const Config& config);
    Config GetConfig() const;
    IntervalTree<uint64_t, uint32_t>& GetModuleIntervalTree();
    IntervalTree<uint64_t, uint32_t>& GetMemoryIntervalTree();
    BitmapIndex& GetBitmapIndex();
    RangeMap<uint64_t, uint32_t>& GetModuleRangeMap();
    RangeMap<uint64_t, uint32_t>& GetMemoryRangeMap();
    void Reset();
    Statistics GetStatistics() const;

private:
    PerformanceOptimizer();
    Config config_;
    IntervalTree<uint64_t, uint32_t> intervalTreeModule_;
    IntervalTree<uint64_t, uint32_t> intervalTreeMemory_;
    BitmapIndex bitmapIndex_;
    RangeMap<uint64_t, uint32_t> rangeMapModule_;
    RangeMap<uint64_t, uint32_t> rangeMapMemory_;
};
}
}

#endif