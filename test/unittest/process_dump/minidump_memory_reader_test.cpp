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

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpMemoryReaderTest : public testing::Test {};

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    char buf[4] = {};
    EXPECT_FALSE(reader.ReadBytes(buf, 4));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest002, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    char buf[4] = {};
    EXPECT_FALSE(reader.ReadBytes(buf, 4));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest003, TestSize.Level2)
{
    std::string data = "ABCD";
    auto reader = MakeReader(data);
    char buf[4] = {};
    EXPECT_TRUE(reader->ReadBytes(buf, 4));
    EXPECT_EQ(buf[0], 'A');
    EXPECT_EQ(buf[3], 'D');
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest004, TestSize.Level2)
{
    std::string data = "AB";
    auto reader = MakeReader(data);
    char buf[4] = {};
    EXPECT_FALSE(reader->ReadBytes(buf, 4));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest005, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    EXPECT_FALSE(reader.SeekSet(0));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest006, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("ABCDEF", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    EXPECT_FALSE(reader.SeekSet(0));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest007, TestSize.Level2)
{
    std::string data = "ABCDEF";
    auto reader = MakeReader(data);
    EXPECT_TRUE(reader->SeekSet(3));
    char buf[3] = {};
    EXPECT_TRUE(reader->ReadBytes(buf, 3));
    EXPECT_EQ(buf[0], 'D');
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest008, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    EXPECT_EQ(reader.Tell(), -1);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest009, TestSize.Level2)
{
    std::string data = "ABCDEF";
    auto reader = MakeReader(data);
    EXPECT_TRUE(reader->SeekSet(3));
    EXPECT_EQ(reader->Tell(), 3);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest010, TestSize.Level2)
{
    std::vector<uint16_t> empty;
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::string result = reader.ConvertUTF16ToUTF8(empty);
    EXPECT_TRUE(result.empty());
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest011, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> ascii = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    std::string result = reader.ConvertUTF16ToUTF8(ascii);
    EXPECT_EQ(result, "Hello");
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest012, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> twoByte = {0x00E9};
    std::string result = reader.ConvertUTF16ToUTF8(twoByte);
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], static_cast<char>(0xC3));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest013, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> threeByte = {0x4E2D};
    std::string result = reader.ConvertUTF16ToUTF8(threeByte);
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], static_cast<char>(0xE4));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest014, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> surrogatePair = {0xD852, 0xDE52};
    std::string result = reader.ConvertUTF16ToUTF8(surrogatePair);
    EXPECT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], static_cast<char>(0xF0));
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest015, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> loneLow = {0xDC00};
    std::string result = reader.ConvertUTF16ToUTF8(loneLow);
    EXPECT_TRUE(result.empty());
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest016, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> loneHighNoLow = {0xD800};
    std::string result = reader.ConvertUTF16ToUTF8(loneHighNoLow);
    EXPECT_TRUE(result.empty());
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest017, TestSize.Level2)
{
    std::string data(sizeof(uint32_t) + 4, '\0');
    uint32_t length = 4;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    data[4] = 0x41;
    data[5] = 0x00;
    data[6] = 0x42;
    data[7] = 0x00;
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_NE(str, nullptr);
    EXPECT_EQ(*str, "AB");
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest018, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 0;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_EQ(str, nullptr);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest019, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 3;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_EQ(str, nullptr);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest020, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 2;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_EQ(str, nullptr);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest021, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 2;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    std::string utf8Str;
    auto reader = MakeReader(data);
    bool ret = reader->ReadUTF8String(0, &utf8Str);
    EXPECT_FALSE(ret);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderLastErrorTest001, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    EXPECT_EQ(reader.GetLastError().GetError(), MinidumpError::SUCCESS);
    reader.SeekSet(0);
    EXPECT_NE(reader.GetLastError().GetError(), MinidumpError::SUCCESS);
}

HWTEST_F(MinidumpMemoryReaderTest, MemReaderShortReadTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("AB", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    char buf[10] = {};
    EXPECT_FALSE(reader.ReadBytes(buf, 10));
    MinidumpPerfMonitor::Instance().ResetStats();
}

} // namespace HiviewDFX
} // namespace OHOS
