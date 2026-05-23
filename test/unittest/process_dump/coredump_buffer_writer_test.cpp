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
#include <cstring>
#include <string>
#include <vector>

#include "coredump_buffer_writer.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class CoredumpBufferWriterTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: CoredumpBufferWriterWrite
 * @tc.desc: test CoredumpBufferWriter writing data to buffer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_Write_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_Write_001: start.";
    std::vector<char> buffer(1024, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    std::string data = "hello";
    bool ret = writer.Write(data.c_str(), data.size());
    ASSERT_TRUE(ret);
    ASSERT_EQ(writer.GetOffset(), data.size());
    ASSERT_EQ(memcmp(buffer.data(), "hello", 5), 0);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_Write_001: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest002
 * @tc.desc: test CoredumpBufferWriter writing data that overflows buffer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_WriteOverflow_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteOverflow_002: start.";
    std::vector<char> buffer(10, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    std::string data = "hello_world_overflow";
    bool ret = writer.Write(data.c_str(), data.size());
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteOverflow_002: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest003
 * @tc.desc: test CoredumpBufferWriter writing with null base pointer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_WriteNullBase_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteNullBase_003: start.";
    CoredumpBufferWriter writer(nullptr, 1024);
    std::string data = "hello";
    bool ret = writer.Write(data.c_str(), data.size());
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteNullBase_003: end.";
}

/**
 * @tc.name: CoredumpBufferWriterAdvance
 * @tc.desc: test CoredumpBufferWriter advancing offset in buffer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_Advance_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_Advance_004: start.";
    std::vector<char> buffer(1024, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    bool ret = writer.Advance(100);
    ASSERT_TRUE(ret);
    ASSERT_EQ(writer.GetOffset(), 100);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_Advance_004: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest005
 * @tc.desc: test CoredumpBufferWriter advancing offset beyond buffer size
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_AdvanceOverflow_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_AdvanceOverflow_005: start.";
    std::vector<char> buffer(100, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    bool ret = writer.Advance(200);
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_AdvanceOverflow_005: end.";
}

/**
 * @tc.name: CoredumpBufferWriterWriteAt
 * @tc.desc: test CoredumpBufferWriter writing data at specific offset
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_WriteAt_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteAt_006: start.";
    std::vector<char> buffer(1024, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    std::string data = "test_data";
    bool ret = writer.WriteAt(data.c_str(), data.size(), 50);
    ASSERT_TRUE(ret);
    ASSERT_EQ(memcmp(buffer.data() + 50, "test_data", data.size()), 0);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteAt_006: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest007
 * @tc.desc: test CoredumpBufferWriter writing at offset that overflows buffer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_WriteAtOverflow_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteAtOverflow_007: start.";
    std::vector<char> buffer(100, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    std::string data = "test_data";
    bool ret = writer.WriteAt(data.c_str(), data.size(), 95);
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteAtOverflow_007: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest008
 * @tc.desc: test CoredumpBufferWriter writing at offset with null base pointer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_WriteAtNullBase_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteAtNullBase_008: start.";
    CoredumpBufferWriter writer(nullptr, 1024);
    std::string data = "test";
    bool ret = writer.WriteAt(data.c_str(), data.size(), 10);
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_WriteAtNullBase_008: end.";
}

/**
 * @tc.name: CoredumpBufferWriterSetCurrent
 * @tc.desc: test CoredumpBufferWriter setting current position in buffer
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_SetCurrent_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_SetCurrent_009: start.";
    std::vector<char> buffer(1024, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    bool ret = writer.SetCurrent(buffer.data() + 500);
    ASSERT_TRUE(ret);
    ASSERT_EQ(writer.GetOffset(), 500);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_SetCurrent_009: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest010
 * @tc.desc: test CoredumpBufferWriter setting current position out of buffer range
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_SetCurrentOutOfRange_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_SetCurrentOutOfRange_010: start.";
    std::vector<char> buffer(1024, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    bool ret = writer.SetCurrent(buffer.data() - 1);
    ASSERT_FALSE(ret);
    ret = writer.SetCurrent(buffer.data() + buffer.size() + 1);
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_SetCurrentOutOfRange_010: end.";
}

/**
 * @tc.name: CoredumpBufferWriterTest011
 * @tc.desc: test CoredumpBufferWriter resetting offset to zero
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpBufferWriterTest, CoredumpBufferWriter_Reset_011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_Reset_011: start.";
    std::vector<char> buffer(1024, 0);
    CoredumpBufferWriter writer(buffer.data(), buffer.size());
    writer.Write("hello", 5);
    ASSERT_EQ(writer.GetOffset(), 5);
    writer.Reset();
    ASSERT_EQ(writer.GetOffset(), 0);
    GTEST_LOG_(INFO) << "CoredumpBufferWriter_Reset_011: end.";
}
}