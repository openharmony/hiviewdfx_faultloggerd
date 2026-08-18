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

#include "minidump_optimizer.h"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <thread>

#include "dfx_log.h"
#include "minidump_index.h"

namespace OHOS {
namespace HiviewDFX {

template<typename AddressType, typename EntryType>
struct IntervalNode {
    AddressType low;
    AddressType high;
    AddressType max;
    EntryType value;
    IntervalNode* left;
    IntervalNode* right;
    int height;

    IntervalNode(AddressType l, AddressType h, EntryType v)
        : low(l), high(h), max(h), value(v), left(nullptr), right(nullptr), height(1)
    {}
};

template<typename NodeType>
NodeType* NodeArena<NodeType>::Allocate()
{
    constexpr size_t chunkBytes = CHUNK_BYTES;
    if (chunks_.empty() || chunkUsed_ + sizeof(NodeType) > chunkBytes) {
        chunks_.push_back(std::make_unique<char[]>(chunkBytes));
        chunkUsed_ = 0;
    }
    char* mem = chunks_.back().get() + chunkUsed_;
    chunkUsed_ += sizeof(NodeType);
    return reinterpret_cast<NodeType*>(mem);
}

template<typename NodeType>
void NodeArena<NodeType>::Clear()
{
    chunks_.clear();
    chunkUsed_ = 0;
}

template <typename AddressType, typename EntryType>
RangeMap<AddressType, EntryType>::RangeMap() : high_(0) {}

template <typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::StoreRange(AddressType base, AddressType size, EntryType value)
{
    if (size == 0) {
        return false;
    }
    std::pair<AddressType, bool> result;
    result.second = __builtin_add_overflow(base, size, &result.first);
    if (result.second) {
        return false;
    }
    AddressType high = result.first - 1;

    MapConstIterator it = map_.lower_bound(base);
    if (it != map_.end()) {
        if (it->first <= high) {
            return false;
        }
    }
    if (it != map_.begin()) {
        --it;
        if (it->second.high >= base) {
            return false;
        }
    }

    map_[base] = RangeMapEntry{base, high, std::move(value)};
    if (high > high_) {
        high_ = high;
    }
    return true;
}

template <typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::RetrieveRange(AddressType address, EntryType* value) const
{
    if (map_.empty()) {
        return false;
    }
    MapConstIterator it = map_.upper_bound(address);
    if (it == map_.begin()) {
        return false;
    }

    --it;
    const RangeMapEntry& entry = it->second;

    if (address >= entry.base && address <= entry.high) {
        if (value != nullptr) {
            *value = entry.value;
        }
        return true;
    }
    return false;
}

template <typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::HasRange(AddressType address) const
{
    return RetrieveRange(address, nullptr);
}

template <typename AddressType, typename EntryType>
void RangeMap<AddressType, EntryType>::Clear() noexcept
{
    map_.clear();
    high_ = 0;
}

template <typename AddressType, typename EntryType>
AddressType RangeMap<AddressType, EntryType>::GetHighestAddress() const noexcept
{
    return high_;
}

template <typename AddressType, typename EntryType>
size_t RangeMap<AddressType, EntryType>::GetRangeSize() const noexcept
{
    return map_.size();
}

template <typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::IsEmpty() const noexcept
{
    return map_.empty();
}

template<typename AddressType, typename EntryType>
IntervalTree<AddressType, EntryType>::IntervalTree() : root_(nullptr), size_(0) {}

template<typename AddressType, typename EntryType>
IntervalTree<AddressType, EntryType>::~IntervalTree() { Clear(); }

template<typename AddressType, typename EntryType>
bool IntervalTree<AddressType, EntryType>::Insert(AddressType low, AddressType high, EntryType value)
{
    if (low > high) return false;
    size_t oldSize = size_;
    root_ = InsertNode(root_, low, high, value);
    return size_ > oldSize;
}

template<typename AddressType, typename EntryType>
bool IntervalTree<AddressType, EntryType>::Search(AddressType address, EntryType* result) const
{
    return SearchNode(root_, address, result);
}

template<typename AddressType, typename EntryType>
std::vector<EntryType> IntervalTree<AddressType, EntryType>::SearchRange(AddressType low, AddressType high) const
{
    std::vector<EntryType> results;
    SearchRangeNode(root_, low, high, results);
    return results;
}

template<typename AddressType, typename EntryType>
void IntervalTree<AddressType, EntryType>::Clear()
{
    root_ = nullptr;
    size_ = 0;
    arena_.Clear();
}

template<typename AddressType, typename EntryType>
size_t IntervalTree<AddressType, EntryType>::Size() const { return size_; }

template<typename AddressType, typename EntryType>
bool IntervalTree<AddressType, EntryType>::Empty() const { return size_ == 0; }

template<typename AddressType, typename EntryType>
AddressType IntervalTree<AddressType, EntryType>::GetMaxAddress() const
{
    return root_ ? root_->max : 0;
}

template<typename AddressType, typename EntryType>
int IntervalTree<AddressType, EntryType>::GetHeight(IntervalNode<AddressType, EntryType>* node) const
{
    return node ? node->height : 0;
}

template<typename AddressType, typename EntryType>
AddressType IntervalTree<AddressType, EntryType>::GetMax(IntervalNode<AddressType, EntryType>* node) const
{
    if (!node) return 0;
    AddressType max = node->high;
    if (node->left) max = std::max(max, node->left->max);
    if (node->right) max = std::max(max, node->right->max);
    return max;
}

template<typename AddressType, typename EntryType>
int IntervalTree<AddressType, EntryType>::GetBalance(IntervalNode<AddressType, EntryType>* node) const
{
    return node ? GetHeight(node->left) - GetHeight(node->right) : 0;
}

template<typename AddressType, typename EntryType>
IntervalNode<AddressType, EntryType>* IntervalTree<AddressType, EntryType>::RotateRight(
    IntervalNode<AddressType, EntryType>* y)
{
    IntervalNode<AddressType, EntryType>* x = y->left;
    IntervalNode<AddressType, EntryType>* T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = std::max(GetHeight(y->left), GetHeight(y->right)) + 1;
    x->height = std::max(GetHeight(x->left), GetHeight(x->right)) + 1;

    y->max = GetMax(y);
    x->max = GetMax(x);

    return x;
}

template<typename AddressType, typename EntryType>
IntervalNode<AddressType, EntryType>* IntervalTree<AddressType, EntryType>::RotateLeft(
    IntervalNode<AddressType, EntryType>* x)
{
    IntervalNode<AddressType, EntryType>* y = x->right;
    IntervalNode<AddressType, EntryType>* T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = std::max(GetHeight(x->left), GetHeight(x->right)) + 1;
    y->height = std::max(GetHeight(y->left), GetHeight(y->right)) + 1;

    x->max = GetMax(x);
    y->max = GetMax(y);

    return y;
}

template<typename AddressType, typename EntryType>
IntervalNode<AddressType, EntryType>* IntervalTree<AddressType, EntryType>::InsertNode(
    IntervalNode<AddressType, EntryType>* node,
    AddressType low, AddressType high, EntryType value)
{
    if (node == nullptr) {
        auto* mem = arena_.Allocate();
        auto* newNode = new (mem) IntervalNode<AddressType, EntryType>(low, high, value);
        size_++;
        return newNode;
    }
    if (low < node->low) {
        node->left = InsertNode(node->left, low, high, value);
    } else if (low > node->low) {
        node->right = InsertNode(node->right, low, high, value);
    } else {
        return node;
    }
    node->height = std::max(GetHeight(node->left), GetHeight(node->right)) + 1;
    node->max = GetMax(node);
    int balance = GetBalance(node);
    if (balance > 1 && low < node->left->low) {
        return RotateRight(node);
    }
    if (balance < -1 && low > node->right->low) {
        return RotateLeft(node);
    }
    if (balance > 1 && low > node->left->low) {
        node->left = RotateLeft(node->left);
        return RotateRight(node);
    }
    if (balance < -1 && low < node->right->low) {
        node->right = RotateRight(node->right);
        return RotateLeft(node);
    }
    return node;
}

template<typename AddressType, typename EntryType>
bool IntervalTree<AddressType, EntryType>::SearchNode(IntervalNode<AddressType, EntryType>* node,
    AddressType address, EntryType* result) const
{
    if (node == nullptr) {
        return false;
    }

    if (address >= node->low && address <= node->high) {
        if (result) *result = node->value;
        return true;
    }

    if (node->left && address <= node->left->max) {
        return SearchNode(node->left, address, result);
    }

    return SearchNode(node->right, address, result);
}

template<typename AddressType, typename EntryType>
void IntervalTree<AddressType, EntryType>::SearchRangeNode(IntervalNode<AddressType, EntryType>* node,
    AddressType low, AddressType high, std::vector<EntryType>& results) const
{
    if (!node) return;

    if (low <= node->high && high >= node->low) {
        results.push_back(node->value);
    }

    if (node->left && low <= node->left->max) {
        SearchRangeNode(node->left, low, high, results);
    }

    if (node->right && node->low <= high) {
        SearchRangeNode(node->right, low, high, results);
    }
}

template<typename AddressType, typename EntryType>
void IntervalTree<AddressType, EntryType>::ClearNode(IntervalNode<AddressType, EntryType>* node)
{
    if (node == nullptr) {
        return;
    }
    ClearNode(node->left);
    ClearNode(node->right);
    delete node;
}

namespace {
constexpr size_t BITS_PER_WORD = 64;
constexpr size_t WORD_ALL_BITS = BITS_PER_WORD - 1;
}

BitmapIndex::BitmapIndex(uint64_t addressRange, uint32_t granularity)
    : granularity_(granularity), bitCount_(0)
{
    if (granularity != 0) {
        uint64_t bitmapSize64 = (addressRange / granularity) + 1;
        constexpr uint64_t maxBitMapSize = (1ULL << 20) + 1;
        if (bitmapSize64 > maxBitMapSize) {
            DFXLOGE("BitmapIndex size too large: %{public}" PRIu64 ", disabling bitmap",
                    bitmapSize64);
            granularity_ = 0;
            return;
        }
        bitCount_ = static_cast<size_t>(bitmapSize64);
        size_t wordCount = (bitCount_ + WORD_ALL_BITS) / BITS_PER_WORD;
        bitmap_.assign(wordCount, 0);
    }
}

void BitmapIndex::MarkRange(uint64_t start, uint64_t end)
{
    if (granularity_ == 0 || bitCount_ == 0) {
        return;
    }
    uint64_t startIdx = start / granularity_;
    uint64_t endIdx = end / granularity_;
    if (startIdx >= bitCount_) {
        return;
    }
    if (endIdx >= bitCount_) {
        endIdx = bitCount_ - 1;
    }

    uint64_t startWord = startIdx / BITS_PER_WORD;
    uint64_t endWord = endIdx / BITS_PER_WORD;
    if (startWord == endWord) {
        uint64_t mask = 0;
        for (uint64_t i = startIdx; i <= endIdx; ++i) {
            mask |= (1ULL << (i % BITS_PER_WORD));
        }
        bitmap_[startWord] |= mask;
    } else {
        uint64_t startBit = startIdx % BITS_PER_WORD;
        uint64_t endBit = endIdx % BITS_PER_WORD;
        bitmap_[startWord] |= ~((1ULL << startBit) - 1);
        for (uint64_t w = startWord + 1; w < endWord; ++w) {
            bitmap_[w] = ~0ULL;
        }
        if (endBit == BITS_PER_WORD - 1) {
            bitmap_[endWord] = ~0ULL;
        } else {
            bitmap_[endWord] |= ((1ULL << (endBit + 1)) - 1);
        }
    }
}

bool BitmapIndex::IsInRange(uint64_t address) const
{
    if (granularity_ == 0 || bitCount_ == 0) {
        return false;
    }
    uint64_t idx = address / granularity_;
    if (idx >= bitCount_) {
        return false;
    }
    uint64_t word = idx / BITS_PER_WORD;
    uint64_t bit = idx % BITS_PER_WORD;
    return (bitmap_[word] & (1ULL << bit)) != 0;
}

uint64_t BitmapIndex::FindNextInRange(uint64_t start) const
{
    if (granularity_ == 0 || bitCount_ == 0) {
        return UINT64_MAX;
    }
    uint64_t idx = start / granularity_;
    if (idx >= bitCount_) {
        return UINT64_MAX;
    }
    uint64_t word = idx / BITS_PER_WORD;
    uint64_t bit = idx % BITS_PER_WORD;
    uint64_t val = bitmap_[word] >> bit;
    if (val != 0) {
        return (word * BITS_PER_WORD + bit + __builtin_ctzll(val)) * granularity_;
    }
    for (size_t w = word + 1; w < bitmap_.size(); ++w) {
        if (bitmap_[w] != 0) {
            return (w * BITS_PER_WORD + __builtin_ctzll(bitmap_[w])) * granularity_;
        }
    }
    return UINT64_MAX;
}

void BitmapIndex::Clear()
{
    std::fill(bitmap_.begin(), bitmap_.end(), 0ULL);
}

size_t BitmapIndex::Size() const
{
    return bitCount_;
}

size_t BitmapIndex::MarkedCount() const
{
    size_t count = 0;
    for (size_t i = 0; i < bitmap_.size(); ++i) {
        count += __builtin_popcountll(bitmap_[i]);
    }
    return count;
}

PerformanceOptimizer& PerformanceOptimizer::Instance()
{
    static PerformanceOptimizer optimizer;
    return optimizer;
}

PerformanceOptimizer::PerformanceOptimizer()
    : intervalTreeModule_(),
      intervalTreeMemory_(),
      bitmapIndex_(0x800000000000ULL,
                   MinidumpConfigManager::Instance().GetConfig().bitmapGranularity),
      adaptiveMemoryIndex_(std::make_unique<AdaptiveAddressIndex>()),
      adaptiveModuleIndex_(std::make_unique<AdaptiveAddressIndex>())
{
}

IntervalTree<uint64_t, uint32_t>& PerformanceOptimizer::GetModuleIntervalTree()
{
    return intervalTreeModule_;
}

IntervalTree<uint64_t, uint32_t>& PerformanceOptimizer::GetMemoryIntervalTree()
{
    return intervalTreeMemory_;
}

BitmapIndex& PerformanceOptimizer::GetBitmapIndex()
{
    return bitmapIndex_;
}

RangeMap<uint64_t, uint32_t>& PerformanceOptimizer::GetModuleRangeMap()
{
    return rangeMapModule_;
}

RangeMap<uint64_t, uint32_t>& PerformanceOptimizer::GetMemoryRangeMap()
{
    return rangeMapMemory_;
}

IAddressIndex* PerformanceOptimizer::GetMemoryAddressIndex()
{
    return adaptiveMemoryIndex_.get();
}

IAddressIndex* PerformanceOptimizer::GetModuleAddressIndex()
{
    return adaptiveModuleIndex_.get();
}

PerformanceOptimizer::Statistics PerformanceOptimizer::GetStatistics() const
{
    Statistics stats;
    stats.intervalTreeModuleSize = intervalTreeModule_.Size();
    stats.intervalTreeMemorySize = intervalTreeMemory_.Size();
    stats.bitmapMarkedCount = bitmapIndex_.MarkedCount();
    stats.rangeMapMemorySize = rangeMapMemory_.GetRangeSize();
    stats.rangeMapModuleSize = rangeMapModule_.GetRangeSize();
    return stats;
}

void PerformanceOptimizer::Reset()
{
    intervalTreeModule_.Clear();
    intervalTreeMemory_.Clear();
    bitmapIndex_ = BitmapIndex(0x800000000000ULL, MinidumpConfigManager::Instance().GetConfig().bitmapGranularity);
    if (adaptiveMemoryIndex_) adaptiveMemoryIndex_->Clear();
    if (adaptiveModuleIndex_) adaptiveModuleIndex_->Clear();
}

template class RangeMap<uint64_t, uint32_t>;
template struct IntervalNode<uint64_t, uint32_t>;
template class NodeArena<IntervalNode<uint64_t, uint32_t>>;
template class IntervalTree<uint64_t, uint32_t>;
}
}