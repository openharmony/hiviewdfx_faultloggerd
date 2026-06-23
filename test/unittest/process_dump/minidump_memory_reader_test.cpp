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

constexpr uint16_t TEST_UTF16_LATIN_E_ACUTE = 0x00E9;
constexpr uint16_t TEST_UTF16_CJK_MIDDLE = 0x4E2D;
constexpr uint16_t TEST_UTF16_SURROGATE_HIGH = 0xD852;
constexpr uint16_t TEST_UTF16_SURROGATE_LOW = 0xDE52;
constexpr uint16_t TEST_UTF16_LONE_LOW_SURROGATE = 0xDC00;
constexpr uint16_t TEST_UTF16_LONE_HIGH_SURROGATE = 0xD800;
constexpr uint16_t TEST_UTF16_VALID_SURROGATE_HIGH = 0xD800;
constexpr uint16_t TEST_UTF16_VALID_SURROGATE_LOW = 0xDC00;

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpMemoryReaderTest : public testing::Test {};

/**
 * @tc.name: MemReaderTest001
 * @tc.desc: test MinidumpMemoryReader ReadBytes with bad stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    char buf[4] = {};
    EXPECT_FALSE(reader.ReadBytes(buf, 4));
}

/**
 * @tc.name: MemReaderTest002
 * @tc.desc: test MinidumpMemoryReader ReadBytes with null stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest002, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    char buf[4] = {};
    EXPECT_FALSE(reader.ReadBytes(buf, 4));
}

/**
 * @tc.name: MemReaderTest003
 * @tc.desc: test MinidumpMemoryReader ReadBytes successfully reads data and verifies content
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest003, TestSize.Level2)
{
    std::string data = "ABCD";
    auto reader = MakeReader(data);
    char buf[4] = {};
    EXPECT_TRUE(reader->ReadBytes(buf, 4));
    EXPECT_EQ(buf[0], 'A');
    EXPECT_EQ(buf[3], 'D');
}

/**
 * @tc.name: MemReaderTest004
 * @tc.desc: test MinidumpMemoryReader ReadBytes with insufficient data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest004, TestSize.Level2)
{
    std::string data = "AB";
    auto reader = MakeReader(data);
    char buf[4] = {};
    EXPECT_FALSE(reader->ReadBytes(buf, 4));
}

/**
 * @tc.name: MemReaderTest005
 * @tc.desc: test MinidumpMemoryReader SeekSet with null stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest005, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    EXPECT_FALSE(reader.SeekSet(0));
}

/**
 * @tc.name: MemReaderTest006
 * @tc.desc: test MinidumpMemoryReader SeekSet with bad stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest006, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("ABCDEF", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    EXPECT_FALSE(reader.SeekSet(0));
}

/**
 * @tc.name: MemReaderTest007
 * @tc.desc: test MinidumpMemoryReader SeekSet successfully seeks and reads correct bytes
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest007, TestSize.Level2)
{
    std::string data = "ABCDEF";
    auto reader = MakeReader(data);
    EXPECT_TRUE(reader->SeekSet(3));
    char buf[3] = {};
    EXPECT_TRUE(reader->ReadBytes(buf, 3));
    EXPECT_EQ(buf[0], 'D');
}

/**
 * @tc.name: MemReaderTest008
 * @tc.desc: test MinidumpMemoryReader Tell with null stream returns -1
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest008, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    EXPECT_EQ(reader.Tell(), -1);
}

/**
 * @tc.name: MemReaderTest009
 * @tc.desc: test MinidumpMemoryReader Tell returns correct position after SeekSet
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest009, TestSize.Level2)
{
    std::string data = "ABCDEF";
    auto reader = MakeReader(data);
    EXPECT_TRUE(reader->SeekSet(3));
    EXPECT_EQ(reader->Tell(), 3);
}

/**
 * @tc.name: MemReaderTest010
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with empty vector returns empty string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest010, TestSize.Level2)
{
    std::vector<uint16_t> empty;
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::string result = reader.ConvertUTF16ToUTF8(empty);
    EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: MemReaderTest011
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with ASCII characters returns correct string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest011, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> ascii = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    std::string result = reader.ConvertUTF16ToUTF8(ascii);
    EXPECT_EQ(result, "Hello");
}

/**
 * @tc.name: MemReaderTest012
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with two-byte UTF-8 output character
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest012, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> twoByte = {TEST_UTF16_LATIN_E_ACUTE};
    std::string result = reader.ConvertUTF16ToUTF8(twoByte);
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], static_cast<char>(0xC3));
}

/**
 * @tc.name: MemReaderTest013
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with three-byte UTF-8 output character
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest013, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> threeByte = {TEST_UTF16_CJK_MIDDLE};
    std::string result = reader.ConvertUTF16ToUTF8(threeByte);
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], static_cast<char>(0xE4));
}

/**
 * @tc.name: MemReaderTest014
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with surrogate pair produces four-byte UTF-8 output
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest014, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> surrogatePair = {TEST_UTF16_SURROGATE_HIGH, TEST_UTF16_SURROGATE_LOW};
    std::string result = reader.ConvertUTF16ToUTF8(surrogatePair);
    EXPECT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], static_cast<char>(0xF0));
}

/**
 * @tc.name: MemReaderTest015
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with lone low surrogate returns empty string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest015, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> loneLow = {TEST_UTF16_LONE_LOW_SURROGATE};
    std::string result = reader.ConvertUTF16ToUTF8(loneLow);
    EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: MemReaderTest016
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with lone high surrogate returns empty string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest016, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> loneHighNoLow = {TEST_UTF16_LONE_HIGH_SURROGATE};
    std::string result = reader.ConvertUTF16ToUTF8(loneHighNoLow);
    EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: MemReaderTest017
 * @tc.desc: test MinidumpMemoryReader ReadString successfully reads UTF-16 string and converts to UTF-8
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MemReaderTest018
 * @tc.desc: test MinidumpMemoryReader ReadString with zero length returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest018, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 0;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_EQ(str, nullptr);
}

/**
 * @tc.name: MemReaderTest019
 * @tc.desc: test MinidumpMemoryReader ReadString with odd byte length returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest019, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 3;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_EQ(str, nullptr);
}

/**
 * @tc.name: MemReaderTest020
 * @tc.desc: test MinidumpMemoryReader ReadString with insufficient data returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTest020, TestSize.Level2)
{
    std::string data(sizeof(uint32_t), '\0');
    uint32_t length = 2;
    (void)memcpy_s(&data[0], data.length(), &length, sizeof(length));
    auto reader = MakeReader(data);
    auto str = reader->ReadString(0);
    EXPECT_EQ(str, nullptr);
}

/**
 * @tc.name: MemReaderTest021
 * @tc.desc: test MinidumpMemoryReader ReadUTF8String with insufficient data returns false
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MemReaderLastErrorTest001
 * @tc.desc: test MinidumpMemoryReader GetLastError returns success initially and error after failed SeekSet
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderLastErrorTest001, TestSize.Level2)
{
    MinidumpMemoryReader reader(nullptr);
    EXPECT_EQ(reader.GetLastError().GetError(), MinidumpError::SUCCESS);
    reader.SeekSet(0);
    EXPECT_NE(reader.GetLastError().GetError(), MinidumpError::SUCCESS);
}

/**
 * @tc.name: MemReaderShortReadTest001
 * @tc.desc: test MinidumpMemoryReader ReadBytes with short data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderShortReadTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("AB", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    char buf[10] = {};
    EXPECT_FALSE(reader.ReadBytes(buf, 10));
    MinidumpPerfMonitor::Instance().ResetStats();
}

/**
 * @tc.name: MemReaderReadUTF8StringValidTest001
 * @tc.desc: test MinidumpMemoryReader ReadUTF8String successfully reads valid UTF-8 string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadUTF8StringValidTest001, TestSize.Level2)
{
    uint32_t byteLength = 4;
    std::string data(sizeof(uint32_t) + byteLength, '\0');
    (void)memcpy_s(&data[0], data.length(), &byteLength, sizeof(byteLength));
    data[4] = 0x41;
    data[5] = 0x00;
    data[6] = 0x42;
    data[7] = 0x00;
    auto reader = MakeReader(data);
    std::string utf8Str;
    EXPECT_TRUE(reader->ReadUTF8String(0, &utf8Str));
    EXPECT_EQ(utf8Str, "AB");
}

/**
 * @tc.name: MemReaderConvertUTF16SurrogatePairTest001
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with valid surrogate pair produces non-empty result
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderConvertUTF16SurrogatePairTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> surrogatePair = {TEST_UTF16_VALID_SURROGATE_HIGH, TEST_UTF16_VALID_SURROGATE_LOW};
    std::string result = reader.ConvertUTF16ToUTF8(surrogatePair);
    EXPECT_FALSE(result.empty());
}

/**
 * @tc.name: MemReaderConvertUTF16LoneHighSurrogateTest001
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with lone high surrogate returns empty string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderConvertUTF16LoneHighSurrogateTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> loneHigh = {TEST_UTF16_LONE_HIGH_SURROGATE};
    std::string result = reader.ConvertUTF16ToUTF8(loneHigh);
    EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: MemReaderConvertUTF16LoneLowSurrogateTest001
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with lone low surrogate returns empty string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderConvertUTF16LoneLowSurrogateTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> loneLow = {TEST_UTF16_LONE_LOW_SURROGATE};
    std::string result = reader.ConvertUTF16ToUTF8(loneLow);
    EXPECT_TRUE(result.empty());
}

/**
 * @tc.name: MemReaderConvertUTF16ThreeByteOutputTest001
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with CJK character produces three-byte UTF-8 output
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderConvertUTF16ThreeByteOutputTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> chinese = {TEST_UTF16_CJK_MIDDLE};
    std::string result = reader.ConvertUTF16ToUTF8(chinese);
    EXPECT_EQ(result.size(), 3u);
}

/**
 * @tc.name: MemReaderConvertUTF16NullTerminatorTest001
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 stops conversion at null terminator
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderConvertUTF16NullTerminatorTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    std::vector<uint16_t> withNull = {0x48, 0x65, 0x00};
    std::string result = reader.ConvertUTF16ToUTF8(withNull);
    EXPECT_EQ(result, "He");
}

/**
 * @tc.name: MemReaderReadStringZeroLengthTest001
 * @tc.desc: test MinidumpMemoryReader ReadString with zero byte length returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadStringZeroLengthTest001, TestSize.Level2)
{
    uint32_t byteLength = 0;
    std::string data(reinterpret_cast<const char*>(&byteLength), sizeof(byteLength));
    auto reader = MakeReader(data);
    auto result = reader->ReadString(0);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name: MemReaderReadStringOddLengthTest001
 * @tc.desc: test MinidumpMemoryReader ReadString with odd byte length returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadStringOddLengthTest001, TestSize.Level2)
{
    uint32_t byteLength = 3;
    std::string data(reinterpret_cast<const char*>(&byteLength), sizeof(byteLength));
    data += "ABC";
    auto reader = MakeReader(data);
    auto result = reader->ReadString(0);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name: MemReaderSeekSetBadStreamTest001
 * @tc.desc: test MinidumpMemoryReader SeekSet on bad stream returns false and sets ERROR_FILE_SEEK
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderSeekSetBadStreamTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    EXPECT_FALSE(reader.SeekSet(0));
    EXPECT_EQ(reader.GetLastError().GetError(), MinidumpError::ERROR_FILE_SEEK);
}

/**
 * @tc.name: MemReaderUTF8CodePointExceedsLimitTest001
 * @tc.desc: test MinidumpMemoryReader ConvertUTF16ToUTF8 with code point exceeding UTF-8 limit returns empty string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderUTF8CodePointExceedsLimitTest001, TestSize.Level2)
{
    MinidumpMemoryReader reader(MakeStream(""));
    std::vector<uint16_t> input = {0xDBFF, 0xE000};
    std::string result = reader.ConvertUTF16ToUTF8(input);
    EXPECT_EQ(result, "");
}

/**
 * @tc.name: MemReaderReadStringSeekSetFailureTest001
 * @tc.desc: test MinidumpMemoryReader ReadString returns nullptr when SeekSet fails on bad stream
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadStringSeekSetFailureTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    auto result = reader.ReadString(0);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name: MemReaderReadStringReadBytesFailureTest001
 * @tc.desc: test MinidumpMemoryReader ReadString returns nullptr when ReadBytes fails at invalid offset
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadStringReadBytesFailureTest001, TestSize.Level2)
{
    uint32_t offset = 100;
    std::string data = "short";
    auto reader = MakeReader(data);
    auto result = reader->ReadString(offset);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name: MemReaderReadStringExceedsMaxStringLengthTest001
 * @tc.desc: test MinidumpMemoryReader ReadString returns nullptr when string length exceeds maxStringLength config
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadStringExceedsMaxStringLengthTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxStringLength = 10;
    mgr.SetConfig(config);

    uint32_t byteLength = 40;
    std::string data(reinterpret_cast<const char*>(&byteLength), sizeof(byteLength));
    for (uint32_t i = 0; i < byteLength; ++i) {
        uint16_t ch = 0x41;
        data += std::string(reinterpret_cast<const char*>(&ch), sizeof(ch));
    }
    auto reader = MakeReader(data);
    auto result = reader->ReadString(0);
    EXPECT_EQ(result, nullptr);
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: MemReaderReadUTF8StringNullStrTest001
 * @tc.desc: test MinidumpMemoryReader ReadUTF8String on bad stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderReadUTF8StringNullStrTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("", std::ios::binary | std::ios::in);
    ss->setstate(std::ios::badbit);
    MinidumpMemoryReader reader(ss);
    std::string utf8Str;
    EXPECT_FALSE(reader.ReadUTF8String(0, &utf8Str));
}

/**
 * @tc.name: MemReaderTellBadStreamTest001
 * @tc.desc: test MinidumpMemoryReader Tell returns -1 on bad stream and valid position on good stream
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryReaderTest, MemReaderTellBadStreamTest001, TestSize.Level2)
{
    auto ss = std::make_shared<std::stringstream>("data", std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    off_t pos = reader.Tell();
    EXPECT_NE(pos, -1);
    ss->setstate(std::ios::badbit);
    off_t badPos = reader.Tell();
    EXPECT_EQ(badPos, -1);
}

} // namespace HiviewDFX
} // namespace OHOS
