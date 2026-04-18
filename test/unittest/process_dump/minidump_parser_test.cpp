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

class MinidumpParserTest : public testing::Test {};
std::string BuildValidMinidumpHeader(uint32_t streamCount, uint32_t checksum = 0)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | 0xa7900000;
    header.numberOfStreams = streamCount;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    header.checksum = checksum;
    header.timeStamp = 0;
    header.flags = 0;
    std::string result(reinterpret_cast<const char*>(&header), sizeof(header));

    for (uint32_t i = 0; i < streamCount; ++i) {
        MDRawDirectory dir = {};
        dir.streamType = MD_STREAM_UNUSED;
        dir.location.dataSize = 0;
        dir.location.rva = 0;
        result += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    }
    return result;
}

HWTEST_F(MinidumpParserTest, ParserTest001, TestSize.Level2)
{
    MinidumpParser parser("/nonexistent/file/path.dmp");
    EXPECT_FALSE(parser.Valid());
    EXPECT_EQ(parser.Path(), "/nonexistent/file/path.dmp");
    EXPECT_EQ(parser.Header(), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryCount(), 0u);
}

HWTEST_F(MinidumpParserTest, ParserTest002, TestSize.Level2)
{
    auto input = std::make_shared<std::ifstream>("/nonexistent/path", std::ios::binary);
    MinidumpParser parser(input);
    EXPECT_FALSE(parser.Valid());
    EXPECT_TRUE(parser.Path().empty());
}

HWTEST_F(MinidumpParserTest, ParserTest003, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    MinidumpPerfMonitor::Instance().ResetStats();
    EXPECT_TRUE(parser.Parse());
    EXPECT_TRUE(parser.Valid());
    EXPECT_NE(parser.Header(), nullptr);
    EXPECT_EQ(parser.Header()->signature, MINIDUMP_HEADER_SIGNATURE);
    EXPECT_EQ(parser.GetDirectoryEntryCount(), 1u);
}

HWTEST_F(MinidumpParserTest, ParserTest004, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = 0xDEADBEEF;
    header.version = MINIDUMP_HEADER_VERSION;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_UNUSED;
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_FALSE(parser.Valid());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_SIGNATURE);
}

HWTEST_F(MinidumpParserTest, ParserTest005, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE_SWAP;
    header.version = MINIDUMP_HEADER_VERSION;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    MDRawDirectory dir = {};
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_ENDIANNESS);
}

HWTEST_F(MinidumpParserTest, ParserTest006, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = 0xFFFF0000;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    MDRawDirectory dir = {};
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_VERSION);
}

HWTEST_F(MinidumpParserTest, ParserTest007, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | 0xa7900000;
    header.numberOfStreams = 0;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_STREAM_COUNT);
}

HWTEST_F(MinidumpParserTest, ParserTest008, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxStreams = 5;
    mgr.SetConfig(config);

    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | 0xa7900000;
    header.numberOfStreams = 10;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_STREAM_COUNT);
    mgr.SetConfig(MinidumpConfig());
}

HWTEST_F(MinidumpParserTest, ParserTest009, TestSize.Level2)
{
    std::string data(sizeof(MDRawHeader) / 2, '\0');
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_FILE_READ);
}

HWTEST_F(MinidumpParserTest, ParserTest010, TestSize.Level2)
{
    MinidumpParser parser{std::shared_ptr<std::istream>()};
    EXPECT_FALSE(parser.Parse());
}

HWTEST_F(MinidumpParserTest, ParserTest011, TestSize.Level2)
{
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_THREAD_LIST), "ThreadList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_THREAD_NAME_LIST), "ThreadNameList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_MODULE_LIST), "ModuleList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_MEMORY_LIST), "MemoryList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_EXCEPTION), "Exception");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_SYSTEM_INFO), "SystemInfo");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_MISC_INFO), "MiscInfo");
    std::string unknown = MinidumpParser(MakeStream("")).GetStreamTypeName(999);
    EXPECT_TRUE(unknown.find("Unknown") != std::string::npos);
}

HWTEST_F(MinidumpParserTest, ParserTest012, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_NE(parser.GetDirectoryEntryAtType(MD_STREAM_UNUSED), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryAtIndex(0)->streamType, MD_STREAM_UNUSED);
}

HWTEST_F(MinidumpParserTest, ParserTest013, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetDirectoryEntryAtType(MD_STREAM_THREAD_LIST), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryAtIndex(999), nullptr);
}

HWTEST_F(MinidumpParserTest, ParserTest014, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_EQ(parser.GetDirectoryEntryAtType(MD_STREAM_THREAD_LIST), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryAtIndex(0), nullptr);
}

HWTEST_F(MinidumpParserTest, ParserTest015, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    uint32_t streamLen = 0;
    EXPECT_FALSE(parser.SeekToStreamType(MD_STREAM_THREAD_LIST, streamLen));
}

HWTEST_F(MinidumpParserTest, ParserTest018, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    parser.Print();
}

HWTEST_F(MinidumpParserTest, ParserTest019, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_FALSE(parser.Parse());
    EXPECT_FALSE(parser.Valid());
    parser.Print();
}

HWTEST_F(MinidumpParserTest, ParserTest020, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetThreadList(), nullptr);
    EXPECT_EQ(parser.GetThreadNameList(), nullptr);
    EXPECT_EQ(parser.GetModuleList(), nullptr);
    EXPECT_EQ(parser.GetMemoryList(), nullptr);
    EXPECT_EQ(parser.GetException(), nullptr);
    EXPECT_EQ(parser.GetSystemInfo(), nullptr);
    EXPECT_EQ(parser.GetMiscInfo(), nullptr);
}

HWTEST_F(MinidumpParserTest, ParserTest021, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_EQ(parser.GetThreadList(), nullptr);
}

HWTEST_F(MinidumpParserTest, ParserValidateChecksum001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetEnableChecksumValidation(false);
    std::string data = BuildValidMinidumpHeader(1, 12345);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    mgr.SetConfig(MinidumpConfig());
}

HWTEST_F(MinidumpParserTest, ParserTest022, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | 0xa7900000;
    header.numberOfStreams = 2;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));

    MDRawDirectory dir1 = {};
    dir1.streamType = MD_STREAM_THREAD_LIST;
    dir1.location.dataSize = 10;
    dir1.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory) * 2;
    data += std::string(reinterpret_cast<const char*>(&dir1), sizeof(dir1));

    MDRawDirectory dir2 = {};
    dir2.streamType = MD_STREAM_THREAD_LIST;
    dir2.location.dataSize = 10;
    dir2.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory) * 2 + 10;
    data += std::string(reinterpret_cast<const char*>(&dir2), sizeof(dir2));

    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_FALSE(parser.Parse());
    EXPECT_NE(parser.GetLastError().GetError(), MinidumpError::SUCCESS);
}

} // namespace HiviewDFX
} // namespace OHOS
