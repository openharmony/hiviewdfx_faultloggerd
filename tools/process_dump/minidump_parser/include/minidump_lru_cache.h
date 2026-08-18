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

#ifndef MINIDUMP_LRU_CACHE_H
#define MINIDUMP_LRU_CACHE_H

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

namespace OHOS {
namespace HiviewDFX {

template <typename KeyType, typename ValueType, typename Hash = std::hash<KeyType>>
class LruCache {
public:
    explicit LruCache(size_t capacity) : capacity_(capacity) {}

    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    void Put(const KeyType& key, const ValueType& value)
    {
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->second = value;
            MoveToFront(it->second);
            return;
        }
        if (items_.size() >= capacity_) {
            index_.erase(items_.back().first);
            items_.pop_back();
        }
        items_.push_front({key, value});
        index_[key] = items_.begin();
    }

    std::optional<ValueType> Get(const KeyType& key)
    {
        auto it = index_.find(key);
        if (it == index_.end()) {
            return std::nullopt;
        }
        MoveToFront(it->second);
        return it->second->second;
    }

    bool Contains(const KeyType& key) const
    {
        return index_.find(key) != index_.end();
    }

    void Clear()
    {
        items_.clear();
        index_.clear();
    }

    size_t Size() const { return items_.size(); }
    size_t Capacity() const { return capacity_; }

    void SetCapacity(size_t capacity)
    {
        capacity_ = capacity;
        while (items_.size() > capacity_) {
            index_.erase(items_.back().first);
            items_.pop_back();
        }
    }

private:
    using ListItem = std::pair<KeyType, ValueType>;
    using ListIterator = typename std::list<ListItem>::iterator;

    void MoveToFront(ListIterator it)
    {
        items_.splice(items_.begin(), items_, it);
    }

    size_t capacity_;
    std::list<ListItem> items_;
    std::unordered_map<KeyType, ListIterator, Hash> index_;
};

}
}

#endif
