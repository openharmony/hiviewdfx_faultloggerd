/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <string>

#include "minidump_lru_cache.h"

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpLruCacheTest : public testing::Test {};

/**
 * @tc.name: LruCachePutGetTest001
 * @tc.desc: test LruCache Put and Get with single entry returns correct value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCachePutGetTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(4);
    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_EQ(cache.Capacity(), 4u);
    cache.Put(0x1000, 42u);
    EXPECT_EQ(cache.Size(), 1u);
    auto result = cache.Get(0x1000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42u);
}

/**
 * @tc.name: LruCacheGetNotFoundTest001
 * @tc.desc: test LruCache Get with missing key returns nullopt
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheGetNotFoundTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(4);
    cache.Put(0x1000, 42u);
    auto result = cache.Get(0x2000);
    EXPECT_FALSE(result.has_value());
}

/**
 * @tc.name: LruCacheContainsTest001
 * @tc.desc: test LruCache Contains returns true for existing key and false for missing key
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheContainsTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(4);
    cache.Put(0x1000, 1u);
    EXPECT_TRUE(cache.Contains(0x1000));
    EXPECT_FALSE(cache.Contains(0x2000));
}

/**
 * @tc.name: LruCacheEvictionTest001
 * @tc.desc: test LruCache evicts least recently used entry when capacity exceeded
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheEvictionTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(2);
    cache.Put(0x1000, 1u);
    cache.Put(0x2000, 2u);
    EXPECT_EQ(cache.Size(), 2u);
    cache.Put(0x3000, 3u);
    EXPECT_EQ(cache.Size(), 2u);
    EXPECT_FALSE(cache.Contains(0x1000));
    EXPECT_TRUE(cache.Contains(0x2000));
    EXPECT_TRUE(cache.Contains(0x3000));
}

/**
 * @tc.name: LruCacheUpdateExistingKeyTest001
 * @tc.desc: test LruCache Put with existing key updates value and moves to front
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheUpdateExistingKeyTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(2);
    cache.Put(0x1000, 1u);
    cache.Put(0x2000, 2u);
    cache.Put(0x1000, 10u);
    EXPECT_EQ(cache.Size(), 2u);
    auto result = cache.Get(0x1000);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 10u);
    cache.Put(0x3000, 3u);
    EXPECT_FALSE(cache.Contains(0x2000));
    EXPECT_TRUE(cache.Contains(0x1000));
    EXPECT_TRUE(cache.Contains(0x3000));
}

/**
 * @tc.name: LruCacheLRUOrderTest001
 * @tc.desc: test LruCache maintains LRU order after Get moves entry to front
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheLRUOrderTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(3);
    cache.Put(0x1000, 1u);
    cache.Put(0x2000, 2u);
    cache.Put(0x3000, 3u);
    EXPECT_TRUE(cache.Get(0x1000).has_value());
    cache.Put(0x4000, 4u);
    EXPECT_TRUE(cache.Contains(0x1000));
    EXPECT_FALSE(cache.Contains(0x2000));
    EXPECT_TRUE(cache.Contains(0x3000));
    EXPECT_TRUE(cache.Contains(0x4000));
}

/**
 * @tc.name: LruCacheClearTest001
 * @tc.desc: test LruCache Clear resets size to zero
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheClearTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(4);
    cache.Put(0x1000, 1u);
    cache.Put(0x2000, 2u);
    EXPECT_EQ(cache.Size(), 2u);
    cache.Clear();
    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_FALSE(cache.Contains(0x1000));
}

/**
 * @tc.name: LruCacheSetCapacityTest001
 * @tc.desc: test LruCache SetCapacity reduces capacity and evicts excess entries
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheSetCapacityTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(4);
    cache.Put(0x1000, 1u);
    cache.Put(0x2000, 2u);
    cache.Put(0x3000, 3u);
    cache.SetCapacity(2);
    EXPECT_EQ(cache.Capacity(), 2u);
    EXPECT_EQ(cache.Size(), 2u);
    EXPECT_FALSE(cache.Contains(0x1000));
}

/**
 * @tc.name: LruCacheSetCapacityIncreaseTest001
 * @tc.desc: test LruCache SetCapacity increases capacity without losing entries
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheSetCapacityIncreaseTest001, TestSize.Level2)
{
    LruCache<uint64_t, uint32_t> cache(2);
    cache.Put(0x1000, 1u);
    cache.Put(0x2000, 2u);
    cache.SetCapacity(4);
    EXPECT_EQ(cache.Capacity(), 4u);
    EXPECT_EQ(cache.Size(), 2u);
    EXPECT_TRUE(cache.Contains(0x1000));
    EXPECT_TRUE(cache.Contains(0x2000));
}

/**
 * @tc.name: LruCacheStringKeyTest001
 * @tc.desc: test LruCache with string keys and string values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLruCacheTest, LruCacheStringKeyTest001, TestSize.Level2)
{
    LruCache<std::string, std::string> cache(4);
    cache.Put("key1", "value1");
    cache.Put("key2", "value2");
    auto result = cache.Get("key1");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
    EXPECT_FALSE(cache.Get("missing").has_value());
}

} // namespace HiviewDFX
} // namespace OHOS
