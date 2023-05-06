/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_HIVIEWDFX_HASHLIST_H
#define OHOS_HIVIEWDFX_HASHLIST_H

#include <unordered_map>

namespace OHOS {
namespace HiviewDFX {
class Link {
public:
    Link() = default;
    ~Link() = default;
    Link(const Link &link) : prev_ {link.prev_}, next_ {link.next_} {}
    Link(Link &&link) : prev_ {link.prev_}, next_ {link.next_}
    {
        link.prev_ = nullptr;
        link.next_ = nullptr;
    }
    Link &operator=(const Link &link)
    {
        prev_ = link.prev_;
        next_ = link.next_;
        return *this;
    }
    Link &operator=(Link &&link)
    {
        prev_ = link.prev_;
        link.prev_ = nullptr;
        next_ = link.next_;
        link.next_ = nullptr;
        return *this;
    }
    Link *prev_ {nullptr};
    Link *next_ {nullptr};
};

template<typename Key, typename Val>
class LinkNode {
public:
    Link link_ {};
    Key key_ {};
    Val val_ {};

    explicit LinkNode() = default;
    ~LinkNode() = default;
    explicit LinkNode(const Key &key);
    explicit LinkNode(const Key &key, const Val &val);
    explicit LinkNode(const Key &key, Val &&val);
    LinkNode(const LinkNode &node);
    LinkNode(LinkNode &&node);
    LinkNode &operator=(const LinkNode &node);
    LinkNode &operator=(LinkNode &&node);
    static LinkNode<Key, Val> *GetLinkNode(Val *pval);
    static LinkNode<Key, Val> *GetLinkNode(Link *plink);
};

template<typename Key, typename Val>
class HashList {
public:
    class Iterator {
    public:
        Iterator() = default;
        ~Iterator() = default;
        explicit Iterator(LinkNode<Key, Val> *pnode, HashList *phashList);
        explicit Iterator(const LinkNode<Key, Val> *pnode, const HashList *phashList);
        Iterator(const Iterator &itr);
        Iterator(Iterator &&itr);
        Iterator &operator=(const Iterator &itr);
        Iterator &operator=(Iterator &&itr);
        Iterator &operator++() noexcept;
        Iterator operator++(int) noexcept;
        Iterator &operator--() noexcept;
        Iterator operator--(int) noexcept;
        bool operator<(const Iterator &itr) const noexcept;
        bool operator==(const Iterator &itr) const noexcept;
        Val &operator*();
        const Val &operator*() const;
        Val *operator->();
        const Val *operator->() const;
        void swap(HashList<Key, Val>::Iterator &other);
        LinkNode<Key, Val> *GetNode() const
        {
            return pnode_;
        }

    private:
        bool IsDangled() const noexcept
        {
            return phashList_ == nullptr;
        }

        LinkNode<Key, Val> *pnode_ {nullptr};
        HashList *phashList_ {nullptr};
    };

    class ReverseIterator {
    public:
        ReverseIterator() = default;
        ~ReverseIterator() = default;
        explicit ReverseIterator(LinkNode<Key, Val> *pnode, HashList *phashList);
        explicit ReverseIterator(const LinkNode<Key, Val> *pnode, const HashList *phashList);
        ReverseIterator(const ReverseIterator &itr);
        ReverseIterator(ReverseIterator &&itr);
        ReverseIterator &operator=(const ReverseIterator &itr);
        ReverseIterator &operator=(ReverseIterator &&itr);
        ReverseIterator &operator++() noexcept;
        ReverseIterator operator++(int) noexcept;
        ReverseIterator &operator--() noexcept;
        ReverseIterator operator--(int) noexcept;
        bool operator<(const ReverseIterator &itr) const noexcept;
        bool operator==(const ReverseIterator &itr) const noexcept;
        Val &operator*();
        const Val &operator*() const;
        Val *operator->();
        const Val *operator->() const;
        void swap(HashList<Key, Val>::ReverseIterator &other);

        LinkNode<Key, Val> *GetNode()
        {
            return pnode_;
        }

    private:
        bool IsDangled() const noexcept
        {
            return phashList_ == nullptr;
        }

        LinkNode<Key, Val> *pnode_ {nullptr};
        HashList *phashList_ {nullptr};
    };

public:
    explicit HashList(const std::size_t numItem = 0);
    ~HashList();

    HashList(const HashList &source) = delete;
    HashList &operator=(const HashList &source) = delete;
    HashList(HashList &&source);
    HashList &operator=(HashList &&source);

    // capacity
    inline std::size_t size() const
    {
        return valueTab_.size();
    }
    inline bool empty() const
    {
        return (dataHead_.next_ == &dataHead_) and (dataHead_.prev_ == &dataHead_);
    }
    inline std::size_t capacity() const
    {
        return numItem_;
    }
    inline bool IsFull() const
    {
        return freeHead_.next_ == &freeHead_;
    }
    inline std::size_t count(const Key &key) const
    {
        return valueTab_.count(key);
    }

    int reserve(const std::size_t numItem);
    // iterators
    Iterator begin();
    const Iterator cbegin() const;
    Iterator end();
    const Iterator cend() const;
    ReverseIterator rbegin();
    const ReverseIterator crbegin() const;
    ReverseIterator rend();
    const ReverseIterator crend() const;
    // element access
    Val &front();
    const Val &front() const;
    Val &back(bool prepend = false);
    Val &operator[](const Key &key);
    // lookup
    Iterator find(const Key &key);
    // modifiers
    void push_front(const Key &key, const Val &val);
    void push_front(const Key &key, Val &&val);
    void push_back(const Key &key, const Val &val);
    void push_back(const Key &key, Val &&val);
    void pop_front();
    void pop_back();
    Iterator erase(const Key &key);
    Iterator erase(const Iterator pos);
    Iterator erase(const Iterator first, const Iterator last);

private:
    void MoveToHead(LinkNode<Key, Val> *&pnode);
    void MoveToTail(LinkNode<Key, Val> *&pnode);
    bool MoveNode(const Iterator &pos, LinkNode<Key, Val> *&pnode);
    LinkNode<Key, Val> *AllocateNode(const Key &key);
    LinkNode<Key, Val> *AllocateNode(const Key &key, const Val &val);
    LinkNode<Key, Val> *AllocateNode(const Key &key, Val &&val);
    void ReclaimNode(LinkNode<Key, Val> *&pnode);

    std::size_t numItem_ {0};
    LinkNode<Key, Val> *pData_ {nullptr};
    Link dataHead_ {};
    Link freeHead_ {};
    std::unordered_map<Key, LinkNode<Key, Val> *> valueTab_ {};
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // OHOS_HIVIEWDFX_HASHLIST_H
