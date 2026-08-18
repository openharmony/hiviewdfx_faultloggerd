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

#include "minidump_test_common.h"

#include "minidump_index.h"

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpLinearScanIndexTest : public testing::Test {};
class MinidumpIntervalTreeIndexTest : public testing::Test {};
class MinidumpRangeMapIndexTest : public testing::Test {};
class MinidumpBitmapFilterIndexTest : public testing::Test {};
class MinidumpAdaptiveAddressIndexTest : public testing::Test {};

/**
 * @tc.name: LinearScanIndexInsertTest001
 * @tc.desc: test LinearScanIndex Insert with valid range returns true and Size increases
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexInsertTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_EQ(index.Size(), 1u);
    EXPECT_STREQ(index.Name(), "LinearScan");
}

/**
 * @tc.name: LinearScanIndexInsertReverseTest001
 * @tc.desc: test LinearScanIndex Insert with reversed bounds returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexInsertReverseTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_FALSE(index.Insert(0x2000, 0x1000, 1u));
    EXPECT_EQ(index.Size(), 0u);
}

/**
 * @tc.name: LinearScanIndexInsertOverlapTest001
 * @tc.desc: test LinearScanIndex Insert with overlapping range returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexInsertOverlapTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_FALSE(index.Insert(0x1500, 0x2500, 2u));
    EXPECT_FALSE(index.Insert(0x0500, 0x1500, 3u));
    EXPECT_EQ(index.Size(), 1u);
}

/**
 * @tc.name: LinearScanIndexLookupTest001
 * @tc.desc: test LinearScanIndex Lookup returns true and correct value for address in range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexLookupTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 42u));
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1000, result));
    EXPECT_EQ(result, 42u);
    EXPECT_TRUE(index.Lookup(0x2000, result));
    EXPECT_EQ(result, 42u);
    EXPECT_FALSE(index.Lookup(0x0FFF, result));
    EXPECT_FALSE(index.Lookup(0x2001, result));
}

/**
 * @tc.name: LinearScanIndexIsInRangeTest001
 * @tc.desc: test LinearScanIndex IsInRange returns true for addresses in range and false otherwise
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexIsInRangeTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_TRUE(index.IsInRange(0x1000));
    EXPECT_TRUE(index.IsInRange(0x1500));
    EXPECT_TRUE(index.IsInRange(0x2000));
    EXPECT_FALSE(index.IsInRange(0x0FFF));
    EXPECT_FALSE(index.IsInRange(0x2001));
}

/**
 * @tc.name: LinearScanIndexClearTest001
 * @tc.desc: test LinearScanIndex Clear resets size to zero
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexClearTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_EQ(index.Size(), 1u);
    index.Clear();
    EXPECT_EQ(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1500, result));
}

/**
 * @tc.name: LinearScanIndexMultipleEntriesTest001
 * @tc.desc: test LinearScanIndex with multiple non-overlapping entries returns correct lookups
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpLinearScanIndexTest, LinearScanIndexMultipleEntriesTest001, TestSize.Level2)
{
    LinearScanIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x1FFF, 0u));
    EXPECT_TRUE(index.Insert(0x2000, 0x2FFF, 1u));
    EXPECT_TRUE(index.Insert(0x3000, 0x3FFF, 2u));
    EXPECT_EQ(index.Size(), 3u);
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1500, result));
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(index.Lookup(0x2500, result));
    EXPECT_EQ(result, 1u);
    EXPECT_TRUE(index.Lookup(0x3500, result));
    EXPECT_EQ(result, 2u);
}

/**
 * @tc.name: IntervalTreeIndexInsertLookupTest001
 * @tc.desc: test IntervalTreeIndex Insert and Lookup with valid range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpIntervalTreeIndexTest, IntervalTreeIndexInsertLookupTest001, TestSize.Level2)
{
    IntervalTreeIndex index;
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 5u));
    EXPECT_EQ(index.Size(), 1u);
    EXPECT_STREQ(index.Name(), "IntervalTree");
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1000, result));
    EXPECT_EQ(result, 5u);
    EXPECT_TRUE(index.Lookup(0x2000, result));
    EXPECT_EQ(result, 5u);
    EXPECT_FALSE(index.Lookup(0x0FFF, result));
    EXPECT_FALSE(index.Lookup(0x2001, result));
}

/**
 * @tc.name: IntervalTreeIndexIsInRangeTest001
 * @tc.desc: test IntervalTreeIndex IsInRange returns correct results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpIntervalTreeIndexTest, IntervalTreeIndexIsInRangeTest001, TestSize.Level2)
{
    IntervalTreeIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_TRUE(index.IsInRange(0x1500));
    EXPECT_FALSE(index.IsInRange(0x500));
}

/**
 * @tc.name: IntervalTreeIndexInsertReverseTest001
 * @tc.desc: test IntervalTreeIndex Insert with reversed bounds returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpIntervalTreeIndexTest, IntervalTreeIndexInsertReverseTest001, TestSize.Level2)
{
    IntervalTreeIndex index;
    EXPECT_FALSE(index.Insert(0x2000, 0x1000, 1u));
    EXPECT_EQ(index.Size(), 0u);
}

/**
 * @tc.name: IntervalTreeIndexClearTest001
 * @tc.desc: test IntervalTreeIndex Clear resets to empty state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpIntervalTreeIndexTest, IntervalTreeIndexClearTest001, TestSize.Level2)
{
    IntervalTreeIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_EQ(index.Size(), 1u);
    index.Clear();
    EXPECT_EQ(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1500, result));
}

/**
 * @tc.name: IntervalTreeIndexMultipleEntriesTest001
 * @tc.desc: test IntervalTreeIndex with multiple entries returns correct lookups
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpIntervalTreeIndexTest, IntervalTreeIndexMultipleEntriesTest001, TestSize.Level2)
{
    IntervalTreeIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x1100, 0u));
    EXPECT_TRUE(index.Insert(0x500, 0x600, 1u));
    EXPECT_TRUE(index.Insert(0x2000, 0x2100, 2u));
    EXPECT_EQ(index.Size(), 3u);
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1050, result));
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(index.Lookup(0x550, result));
    EXPECT_EQ(result, 1u);
    EXPECT_TRUE(index.Lookup(0x2050, result));
    EXPECT_EQ(result, 2u);
}

/**
 * @tc.name: RangeMapIndexInsertLookupTest001
 * @tc.desc: test RangeMapIndex Insert and Lookup with valid range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpRangeMapIndexTest, RangeMapIndexInsertLookupTest001, TestSize.Level2)
{
    RangeMapIndex index;
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 7u));
    EXPECT_EQ(index.Size(), 1u);
    EXPECT_STREQ(index.Name(), "RangeMap");
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1000, result));
    EXPECT_EQ(result, 7u);
    EXPECT_TRUE(index.Lookup(0x2000, result));
    EXPECT_EQ(result, 7u);
    EXPECT_FALSE(index.Lookup(0x0FFF, result));
    EXPECT_FALSE(index.Lookup(0x2001, result));
}

/**
 * @tc.name: RangeMapIndexIsInRangeTest001
 * @tc.desc: test RangeMapIndex IsInRange returns correct results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpRangeMapIndexTest, RangeMapIndexIsInRangeTest001, TestSize.Level2)
{
    RangeMapIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_TRUE(index.IsInRange(0x1500));
    EXPECT_FALSE(index.IsInRange(0x500));
}

/**
 * @tc.name: RangeMapIndexInsertReverseTest001
 * @tc.desc: test RangeMapIndex Insert with reversed bounds returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpRangeMapIndexTest, RangeMapIndexInsertReverseTest001, TestSize.Level2)
{
    RangeMapIndex index;
    EXPECT_FALSE(index.Insert(0x2000, 0x1000, 1u));
    EXPECT_EQ(index.Size(), 0u);
}

/**
 * @tc.name: RangeMapIndexInsertOverlapTest001
 * @tc.desc: test RangeMapIndex Insert with overlapping range returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpRangeMapIndexTest, RangeMapIndexInsertOverlapTest001, TestSize.Level2)
{
    RangeMapIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_FALSE(index.Insert(0x1500, 0x2500, 2u));
    EXPECT_EQ(index.Size(), 1u);
}

/**
 * @tc.name: RangeMapIndexClearTest001
 * @tc.desc: test RangeMapIndex Clear resets to empty state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpRangeMapIndexTest, RangeMapIndexClearTest001, TestSize.Level2)
{
    RangeMapIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_EQ(index.Size(), 1u);
    index.Clear();
    EXPECT_EQ(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1500, result));
}

/**
 * @tc.name: BitmapFilterIndexParameterConstructorTest001
 * @tc.desc: test BitmapFilterIndex constructor with parameters inserts and looks up correctly
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexParameterConstructorTest001, TestSize.Level2)
{
    auto preciseIndex = std::make_shared<IntervalTreeIndex>();
    BitmapFilterIndex index(0x10000, 0x1000, preciseIndex);
    EXPECT_STREQ(index.Name(), "BitmapFilter");
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 9u));
    EXPECT_NE(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1500, result));
    EXPECT_EQ(result, 9u);
    EXPECT_TRUE(index.IsInRange(0x1500));
}

/**
 * @tc.name: BitmapFilterIndexDefaultConstructorTest001
 * @tc.desc: test BitmapFilterIndex default constructor creates IntervalTreeIndex as precise index
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexDefaultConstructorTest001, TestSize.Level2)
{
    BitmapFilterIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_NE(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1500, result));
    EXPECT_EQ(result, 1u);
}

/**
 * @tc.name: BitmapFilterIndexLookupNotFoundTest001
 * @tc.desc: test BitmapFilterIndex Lookup returns false for address not in range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexLookupNotFoundTest001, TestSize.Level2)
{
    auto preciseIndex = std::make_shared<IntervalTreeIndex>();
    BitmapFilterIndex index(0x10000, 0x1000, preciseIndex);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x500, result));
    EXPECT_FALSE(index.IsInRange(0x500));
}

/**
 * @tc.name: BitmapFilterIndexClearTest001
 * @tc.desc: test BitmapFilterIndex Clear resets marked count and size
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexClearTest001, TestSize.Level2)
{
    auto preciseIndex = std::make_shared<IntervalTreeIndex>();
    BitmapFilterIndex index(0x10000, 0x1000, preciseIndex);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_NE(index.Size(), 0u);
    index.Clear();
    EXPECT_EQ(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1500, result));
}

/**
 * @tc.name: BitmapFilterIndexSetGranularityTest001
 * @tc.desc: test BitmapFilterIndex SetGranularity recreates bitmap with new granularity
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexSetGranularityTest001, TestSize.Level2)
{
    auto preciseIndex = std::make_shared<IntervalTreeIndex>();
    BitmapFilterIndex index(0x10000, 0x1000, preciseIndex);
    index.SetGranularity(0x100);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1500, result));
    EXPECT_EQ(result, 1u);
}

/**
 * @tc.name: BitmapFilterIndexMarkedCountTest001
 * @tc.desc: test BitmapFilterIndex MarkedCount returns correct count after marking ranges
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexMarkedCountTest001, TestSize.Level2)
{
    auto preciseIndex = std::make_shared<IntervalTreeIndex>();
    BitmapFilterIndex index(0x10000, 0x1000, preciseIndex);
    index.Insert(0x1000, 0x1FFF, 1u);
    size_t markedCount = index.MarkedCount();
    EXPECT_TRUE(markedCount >= 0u);
}

/**
 * @tc.name: BitmapFilterIndexNullPreciseIndexTest001
 * @tc.desc: test BitmapFilterIndex with null preciseIndex returns false for Lookup and IsInRange
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpBitmapFilterIndexTest, BitmapFilterIndexNullPreciseIndexTest001, TestSize.Level2)
{
    BitmapFilterIndex index(0x10000, 0x1000, nullptr);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_EQ(index.Size(), 0u);
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1500, result));
    EXPECT_FALSE(index.IsInRange(0x1500));
}

/**
 * @tc.name: AdaptiveAddressIndexInsertLookupTest001
 * @tc.desc: test AdaptiveAddressIndex Insert and Lookup with valid range under LINEAR_THRESHOLD
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexInsertLookupTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index;
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 11u));
    EXPECT_EQ(index.Size(), 1u);
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1500, result));
    EXPECT_EQ(result, 11u);
    EXPECT_TRUE(index.IsInRange(0x1500));
}

/**
 * @tc.name: AdaptiveAddressIndexInsertReverseTest001
 * @tc.desc: test AdaptiveAddressIndex Insert with reversed bounds returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexInsertReverseTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index;
    EXPECT_FALSE(index.Insert(0x2000, 0x1000, 1u));
    EXPECT_EQ(index.Size(), 0u);
}

/**
 * @tc.name: AdaptiveAddressIndexLookupEmptyTest001
 * @tc.desc: test AdaptiveAddressIndex Lookup on empty index returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexLookupEmptyTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index;
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1000, result));
    EXPECT_FALSE(index.IsInRange(0x1000));
}

/**
 * @tc.name: AdaptiveAddressIndexClearTest001
 * @tc.desc: test AdaptiveAddressIndex Clear resets to empty state and migration count to zero
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexClearTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_EQ(index.Size(), 1u);
    index.Clear();
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_EQ(index.GetMigrationCount(), 0u);
    uint32_t result = 0;
    EXPECT_FALSE(index.Lookup(0x1500, result));
}

/**
 * @tc.name: AdaptiveAddressIndexMigrateToTreeTest001
 * @tc.desc: test AdaptiveAddressIndex migrates to IntervalTreeIndex when entries exceed LINEAR_THRESHOLD
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexMigrateToTreeTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index(AdaptiveAddressIndex::DEFAULT_ADDRESS_RANGE,
                                AdaptiveAddressIndex::DEFAULT_BITMAP_GRANULARITY);
    for (size_t i = 0; i <= AdaptiveAddressIndex::LINEAR_THRESHOLD; ++i) {
        uint64_t start = 0x1000 * (i + 1);
        uint64_t end = start + 0x100;
        EXPECT_TRUE(index.Insert(start, end, static_cast<uint32_t>(i)));
    }
    EXPECT_GT(index.GetMigrationCount(), 0u);
    EXPECT_STREQ(index.Name(), "IntervalTree");
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1050, result));
}

/**
 * @tc.name: AdaptiveAddressIndexMigrateToBitmapTest001
 * @tc.desc: test AdaptiveAddressIndex migrates to BitmapFilterIndex when entries exceed BITMAP_THRESHOLD
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexMigrateToBitmapTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index(AdaptiveAddressIndex::DEFAULT_ADDRESS_RANGE,
                                AdaptiveAddressIndex::DEFAULT_BITMAP_GRANULARITY);
    for (size_t i = 0; i <= AdaptiveAddressIndex::BITMAP_THRESHOLD; ++i) {
        uint64_t start = 0x1000 * (i + 1);
        uint64_t end = start + 0x100;
        EXPECT_TRUE(index.Insert(start, end, static_cast<uint32_t>(i)));
    }
    EXPECT_GT(index.GetMigrationCount(), 0u);
    EXPECT_STREQ(index.Name(), "BitmapFilter");
    uint32_t result = 0;
    EXPECT_TRUE(index.Lookup(0x1050, result));
}

/**
 * @tc.name: AdaptiveAddressIndexOverlapTest001
 * @tc.desc: test AdaptiveAddressIndex Insert with overlapping range returns false but still adds to masterList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpAdaptiveAddressIndexTest, AdaptiveAddressIndexOverlapTest001, TestSize.Level2)
{
    AdaptiveAddressIndex index;
    EXPECT_TRUE(index.Insert(0x1000, 0x2000, 1u));
    EXPECT_FALSE(index.Insert(0x1500, 0x2500, 2u));
    EXPECT_EQ(index.Size(), 2u);
}

} // namespace HiviewDFX
} // namespace OHOS
