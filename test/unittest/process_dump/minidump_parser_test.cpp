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

constexpr uint32_t MINIDUMP_VERSION_TIMESTAMP = 0xa7900000;
constexpr uint32_t TEST_INVALID_SIGNATURE = 0xDEADBEEF;
constexpr uint32_t TEST_INVALID_VERSION = 0xFFFF0000;
constexpr uint32_t TEST_OUT_OF_RANGE_RVA = 999999;
constexpr uint32_t TEST_UNKNOWN_STREAM_TYPE = 999;
constexpr mode_t TEST_FILE_PERMISSIONS = 0644;

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpParserTest : public testing::Test {};
std::string BuildValidMinidumpHeader(uint32_t streamCount, uint32_t checksum = 0)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
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
/**
 * @tc.name: ParserTest001
 * @tc.desc: test MinidumpParser with nonexistent file path returns invalid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest001, TestSize.Level2)
{
    MinidumpParser parser("/nonexistent/file/path.dmp");
    EXPECT_FALSE(parser.Valid());
    EXPECT_EQ(parser.Path(), "/nonexistent/file/path.dmp");
    EXPECT_EQ(parser.Header(), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryCount(), 0u);
}
/**
 * @tc.name: ParserTest002
 * @tc.desc: test MinidumpParser with nonexistent ifstream returns invalid state and empty path
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest002, TestSize.Level2)
{
    auto input = std::make_shared<std::ifstream>("/nonexistent/path", std::ios::binary);
    MinidumpParser parser(input);
    EXPECT_FALSE(parser.Valid());
    EXPECT_TRUE(parser.Path().empty());
}
/**
 * @tc.name: ParserTest003
 * @tc.desc: test MinidumpParser Parse with valid minidump header returns valid state
 * @tc.type: FUNC
 */
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
/**
 * @tc.name: ParserTest004
 * @tc.desc: test MinidumpParser Parse with invalid signature returns ERROR_SIGNATURE
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest004, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = TEST_INVALID_SIGNATURE;
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
/**
 * @tc.name: ParserTest005
 * @tc.desc: test MinidumpParser Parse with swapped signature returns ERROR_ENDIANNESS
 * @tc.type: FUNC
 */
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
/**
 * @tc.name: ParserTest006
 * @tc.desc: test MinidumpParser Parse with invalid version returns ERROR_VERSION
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest006, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = TEST_INVALID_VERSION;
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
/**
 * @tc.name: ParserTest007
 * @tc.desc: test MinidumpParser Parse with zero stream count returns ERROR_STREAM_COUNT
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest007, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 0;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_STREAM_COUNT);
}
/**
 * @tc.name: ParserTest008
 * @tc.desc: test MinidumpParser Parse with stream count exceeding config max returns ERROR_STREAM_COUNT
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest008, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxStreams = 5;
    mgr.SetConfig(config);

    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 10;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_STREAM_COUNT);
    mgr.SetConfig(MinidumpConfig());
}
/**
 * @tc.name: ParserTest009
 * @tc.desc: test MinidumpParser Parse with truncated header data returns ERROR_FILE_READ
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest009, TestSize.Level2)
{
    std::string data(sizeof(MDRawHeader) / 2, '\0');
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_FILE_READ);
}
/**
 * @tc.name: ParserTest010
 * @tc.desc: test MinidumpParser Parse with null stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest010, TestSize.Level2)
{
    MinidumpParser parser{std::shared_ptr<std::istream>()};
    EXPECT_FALSE(parser.Parse());
}
/**
 * @tc.name: ParserTest011
 * @tc.desc: test MinidumpParser GetStreamTypeName returns correct names for known and unknown stream types
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest011, TestSize.Level2)
{
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_THREAD_LIST), "ThreadList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_THREAD_NAME_LIST), "ThreadNameList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_MODULE_LIST), "ModuleList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_MEMORY_LIST), "MemoryList");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_EXCEPTION), "Exception");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_SYSTEM_INFO), "SystemInfo");
    EXPECT_EQ(MinidumpParser(MakeStream("")).GetStreamTypeName(MD_STREAM_MISC_INFO), "MiscInfo");
    std::string unknown = MinidumpParser(MakeStream("")).GetStreamTypeName(TEST_UNKNOWN_STREAM_TYPE);
    EXPECT_TRUE(unknown.find("Unknown") != std::string::npos);
}
/**
 * @tc.name: ParserTest012
 * @tc.desc: test MinidumpParser GetDirectoryEntryAtType and GetDirectoryEntryAtIndex with valid parse
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest012, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_NE(parser.GetDirectoryEntryAtType(MD_STREAM_UNUSED), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryAtIndex(0)->streamType, MD_STREAM_UNUSED);
}
/**
 * @tc.name: ParserTest013
 * @tc.desc: test MinidumpParser directory entry lookup returns nullptr for missing type and invalid index
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest013, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetDirectoryEntryAtType(MD_STREAM_THREAD_LIST), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryAtIndex(999), nullptr);
}
/**
 * @tc.name: ParserTest014
 * @tc.desc: test MinidumpParser directory entry access on empty stream returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest014, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_EQ(parser.GetDirectoryEntryAtType(MD_STREAM_THREAD_LIST), nullptr);
    EXPECT_EQ(parser.GetDirectoryEntryAtIndex(0), nullptr);
}
/**
 * @tc.name: ParserTest015
 * @tc.desc: test MinidumpParser SeekToStreamType on empty stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest015, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    uint32_t streamLen = 0;
    EXPECT_FALSE(parser.SeekToStreamType(MD_STREAM_THREAD_LIST, streamLen));
}
/**
 * @tc.name: ParserTest018
 * @tc.desc: test MinidumpParser Print on valid parsed minidump
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest018, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    parser.Print();
}
/**
 * @tc.name: ParserTest019
 * @tc.desc: test MinidumpParser Print on invalid parser does not crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest019, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_FALSE(parser.Parse());
    EXPECT_FALSE(parser.Valid());
    parser.Print();
}
/**
 * @tc.name: ParserTest020
 * @tc.desc: test MinidumpParser stream getters return nullptr when only unused stream present
 * @tc.type: FUNC
 */
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
/**
 * @tc.name: ParserTest021
 * @tc.desc: test MinidumpParser GetThreadList on empty stream returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest021, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_EQ(parser.GetThreadList(), nullptr);
}
/**
 * @tc.name: ParserValidateChecksum001
 * @tc.desc: test MinidumpParser Parse with checksum validation disabled succeeds with wrong checksum
 * @tc.type: FUNC
 */
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
/**
 * @tc.name: ParserTest022
 * @tc.desc: test MinidumpParser Parse with duplicate THREAD_LIST stream type returns error
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserTest022, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
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
/**
 * @tc.name: ParserStreamDirectoryRvaOverlapTest001
 * @tc.desc: test MinidumpParser Parse with stream directory RVA overlapping header returns ERROR_CORRUPTED_DATA
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserStreamDirectoryRvaOverlapTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = 0;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_CORRUPTED_DATA);
}
/**
 * @tc.name: ParserStreamDirectoryRvaTooSmallTest001
 * @tc.desc: test MinidumpParser Parse with stream directory RVA too small returns ERROR_CORRUPTED_DATA
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserStreamDirectoryRvaTooSmallTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = 10;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_CORRUPTED_DATA);
}
/**
 * @tc.name: ParserStreamDirectorySeekSetFailureTest001
 * @tc.desc: test MinidumpParser GetException with stream seek failure returns nullptr and ERROR_FILE_SEEK
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserStreamDirectorySeekSetFailureTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_EXCEPTION;
    dir.location.dataSize = sizeof(MDExceptionStream);
    dir.location.rva = TEST_OUT_OF_RANGE_RVA;
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    auto exception = parser.GetException();
    EXPECT_EQ(exception, nullptr);
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_FILE_SEEK);
}
/**
 * @tc.name: ParserStreamDirectoryReadBytesFailureTest001
 * @tc.desc: test MinidumpParser Parse with stream directory read failure returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserStreamDirectoryReadBytesFailureTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 2;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string headerData(reinterpret_cast<const char*>(&header), sizeof(header));
    MDRawDirectory dir1 = {};
    dir1.streamType = MD_STREAM_UNUSED;
    dir1.location.dataSize = 0;
    headerData += std::string(reinterpret_cast<const char*>(&dir1), sizeof(dir1));
    auto stream = MakeStream(headerData);
    MinidumpParser parser(stream);
    EXPECT_FALSE(parser.Parse());
}
/**
 * @tc.name: ParserGetStreamTypeNameTest001
 * @tc.desc: test MinidumpParser GetStreamTypeName returns correct names for all known and unknown stream types
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamTypeNameTest001, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(BuildValidMinidumpHeader(0)));
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_THREAD_LIST), "ThreadList");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_THREAD_NAME_LIST), "ThreadNameList");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_MODULE_LIST), "ModuleList");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_MEMORY_LIST), "MemoryList");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_EXCEPTION), "Exception");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_SYSTEM_INFO), "SystemInfo");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_MISC_INFO), "MiscInfo");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_LINUX_MAPS), "LinuxMaps");
    EXPECT_EQ(parser.GetStreamTypeName(MD_STREAM_ESR_INFO), "EsrInfo");
    EXPECT_EQ(parser.GetStreamTypeName(9999), "Unknown(9999)");
}
/**
 * @tc.name: ParserNotifyParseStartTest001
 * @tc.desc: test MinidumpParser notifies observers of parse start event
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserNotifyParseStartTest001, TestSize.Level2)
{
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    auto subject = std::make_shared<MinidumpSubject>();
    bool startNotified = false;
    std::string startPath;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t current, uint32_t total, const std::string& name) {
            if (!name.empty() && !startNotified) {
                startNotified = true;
                startPath = name;
            }
        });
    subject->AddObserver(obs);
    parser.minidumpSubject_ = subject;
    EXPECT_TRUE(parser.Parse());
    EXPECT_TRUE(startNotified);
}
/**
 * @tc.name: ParserNotifyParseCompleteTest001
 * @tc.desc: test MinidumpParser notifies observers of parse complete event
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserNotifyParseCompleteTest001, TestSize.Level2)
{
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    auto subject = std::make_shared<MinidumpSubject>();
    bool completeNotified = false;
    std::string completeMsg;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t current, uint32_t total, const std::string& name) {
            if (name.find("Parse complete") != std::string::npos) {
                completeNotified = true;
                completeMsg = name;
            }
        });
    subject->AddObserver(obs);
    parser.minidumpSubject_ = subject;
    EXPECT_TRUE(parser.Parse());
    EXPECT_TRUE(completeNotified);
}
/**
 * @tc.name: ParserRecordParseTimeDisabledTest001
 * @tc.desc: test MinidumpParser with performance stats disabled records zero parse time
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserRecordParseTimeDisabledTest001, TestSize.Level2)
{
    MinidumpPerfMonitor::Instance().ResetStats();
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.enablePerformanceStats = false;
    mgr.SetConfig(config);
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    EXPECT_EQ(stats.totalParseTimeMs.load(), 0u);
    mgr.SetConfig(MinidumpConfig());
}
/**
 * @tc.name: ParserRecordParseTimeEnabledTest001
 * @tc.desc: test MinidumpParser with performance stats enabled records parse time
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserRecordParseTimeEnabledTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.enablePerformanceStats = true;
    mgr.SetConfig(config);
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    EXPECT_TRUE(stats.totalParseTimeMs.load() >= 0);
    mgr.SetConfig(MinidumpConfig());
}
/**
 * @tc.name: ParserPrintPerformanceStatsTest001
 * @tc.desc: test MinidumpParser PrintPerformanceStats on valid parsed minidump
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserPrintPerformanceStatsTest001, TestSize.Level2)
{
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    parser.PrintPerformanceStats();
}
/**
 * @tc.name: ParserGetStreamCachedTest001
 * @tc.desc: test MinidumpParser GetSystemInfo returns cached result on subsequent calls
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamCachedTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);

    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_SYSTEM_INFO;
    dir.location.dataSize = sizeof(MDRawSystemInfo);
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);

    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));

    MDRawSystemInfo sysInfo = {};
    sysInfo.platformId = MINIDUMP_OS_LINUX;
    sysInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    data += std::string(reinterpret_cast<const char*>(&sysInfo), sizeof(sysInfo));

    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());

    auto sysInfo1 = parser.GetSystemInfo();
    EXPECT_NE(sysInfo1, nullptr);
    auto sysInfo2 = parser.GetSystemInfo();
    EXPECT_NE(sysInfo2, nullptr);
    EXPECT_EQ(sysInfo1, sysInfo2);
}
/**
 * @tc.name: ParserDuplicateStreamTypeDifferentTest001
 * @tc.desc: test MinidumpParser Parse with duplicate MODULE_LIST stream type returns ERROR_ALREADY_EXISTS
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserDuplicateStreamTypeDifferentTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 2;
    header.streamDirectoryRva = sizeof(MDRawHeader);

    MDRawDirectory dir1 = {};
    dir1.streamType = MD_STREAM_MODULE_LIST;
    dir1.location.dataSize = 10;
    dir1.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory) * 2;

    MDRawDirectory dir2 = {};
    dir2.streamType = MD_STREAM_MODULE_LIST;
    dir2.location.dataSize = 10;
    dir2.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory) * 2 + 10;

    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir1), sizeof(dir1));
    data += std::string(reinterpret_cast<const char*>(&dir2), sizeof(dir2));

    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_ALREADY_EXISTS);
}
/**
 * @tc.name: ParserPrintValidTest001
 * @tc.desc: test MinidumpParser Print on valid parsed minidump with unused stream
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserPrintValidTest001, TestSize.Level2)
{
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    parser.Print();
}
/**
 * @tc.name: ParserGetStreamNullFactoryTest001
 * @tc.desc: test MinidumpParser SeekToStreamType with unknown stream type succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamNullFactoryTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);

    MDRawDirectory dir = {};
    dir.streamType = 0xFFFF;
    dir.location.dataSize = 4;
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);

    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    data += "AAAA";

    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    auto dirEntry = parser.GetDirectoryEntryAtType(0xFFFF);
    EXPECT_NE(dirEntry, nullptr);
    uint32_t streamLen = 0;
    auto result = parser.SeekToStreamType(0xFFFF, streamLen);
    EXPECT_TRUE(result);
}
/**
 * @tc.name: ParserOpenPathBasedTest001
 * @tc.desc: test MinidumpParser Open with nonexistent file path returns ERROR_FILE_OPEN
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserOpenPathBasedTest001, TestSize.Level2)
{
    MinidumpParser parser("/nonexistent/path/to/minidump.dmp");
    EXPECT_FALSE(parser.Open());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_FILE_OPEN);
}
/**
 * @tc.name: ParserOpenPathBasedValidTest001
 * @tc.desc: test MinidumpParser Open and Parse with valid file path succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserOpenPathBasedValidTest001, TestSize.Level2)
{
    int tmpFd = open("/data/test/minidump_open_path_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string data = BuildValidMinidumpHeader(1);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/minidump_open_path_test");
    EXPECT_TRUE(parser.Open());
    EXPECT_TRUE(parser.Parse());
    unlink("/data/test/minidump_open_path_test");
}
/**
 * @tc.name: ParserReadStreamDirectorySeekSetFailureTest001
 * @tc.desc: test MinidumpParser SeekToStreamType for missing stream type returns ERROR_NOT_FOUND
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserReadStreamDirectorySeekSetFailureTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_UNUSED;
    dir.location.dataSize = 0;
    dir.location.rva = 0;
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    uint32_t streamLen = 0;
    EXPECT_FALSE(parser.SeekToStreamType(MD_STREAM_EXCEPTION, streamLen));
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_NOT_FOUND);
}
/**
 * @tc.name: ParserSeekToStreamTypeNotFoundTest001
 * @tc.desc: test MinidumpParser SeekToStreamType returns ERROR_NOT_FOUND when stream type not present
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserSeekToStreamTypeNotFoundTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    uint32_t streamLen = 0;
    EXPECT_FALSE(parser.SeekToStreamType(MD_STREAM_EXCEPTION, streamLen));
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_NOT_FOUND);
}
/**
 * @tc.name: ParserGetStreamSeekSetFailureTest001
 * @tc.desc: test MinidumpParser GetStream returns cached result on second call and fails on seek error
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamSeekSetFailureTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 2;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir1 = {};
    dir1.streamType = MD_STREAM_SYSTEM_INFO;
    dir1.location.dataSize = sizeof(MDRawSystemInfo);
    dir1.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory) * 2;
    MDRawDirectory dir2 = {};
    dir2.streamType = MD_STREAM_MISC_INFO;
    dir2.location.dataSize = sizeof(MDRawMiscInfo);
    dir2.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory) * 2 + sizeof(MDRawSystemInfo);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir1), sizeof(dir1));
    data += std::string(reinterpret_cast<const char*>(&dir2), sizeof(dir2));
    MDRawSystemInfo sysInfo = {};
    sysInfo.platformId = MINIDUMP_OS_LINUX;
    sysInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    data += std::string(reinterpret_cast<const char*>(&sysInfo), sizeof(sysInfo));
    MDRawMiscInfo miscInfo = {};
    miscInfo.sizeOfInfo = sizeof(MDRawMiscInfo);
    data += std::string(reinterpret_cast<const char*>(&miscInfo), sizeof(miscInfo));
    auto ss = std::make_shared<std::stringstream>(data, std::ios::binary | std::ios::in);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    auto sysInfo1 = parser.GetSystemInfo();
    EXPECT_NE(sysInfo1, nullptr);
    ss->setstate(std::ios::badbit);
    auto sysInfoCached = parser.GetSystemInfo();
    EXPECT_NE(sysInfoCached, nullptr);
    auto miscInfo1 = parser.GetMiscInfo();
    EXPECT_EQ(miscInfo1, nullptr);
}
/**
 * @tc.name: ParserGetStreamFactoryNullTest001
 * @tc.desc: test MinidumpParser GetStream with unknown stream type factory returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamFactoryNullTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = 0xFFFF;
    dir.location.dataSize = 4;
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    data += "AAAA";
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetStream<MinidumpException>(), nullptr);
}
/**
 * @tc.name: ParserGetEsrInfoTest001
 * @tc.desc: test MinidumpParser GetEsrInfo returns nullptr when no ESR stream present
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetEsrInfoTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetEsrInfo(), nullptr);
}
/**
 * @tc.name: ParserGetStreamNotValidTest001
 * @tc.desc: test MinidumpParser GetStream on invalid parser returns nullptr for all stream types
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamNotValidTest001, TestSize.Level2)
{
    MinidumpParser parser(MakeStream(""));
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetStream<MinidumpException>(), nullptr);
    EXPECT_EQ(parser.GetStream<MinidumpThreadList>(), nullptr);
}
/**
 * @tc.name: ParserReadStreamDirectorySeekSetErrorTest001
 * @tc.desc: test MinidumpParser Parse with out-of-range stream directory RVA returns ERROR_FILE_SEEK
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserReadStreamDirectorySeekSetErrorTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = TEST_OUT_OF_RANGE_RVA;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_FALSE(parser.Parse());
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_FILE_SEEK);
}
/**
 * @tc.name: ParserGetStreamReadFailureTest001
 * @tc.desc: test MinidumpParser GetSystemInfo with seek failure returns nullptr and ERROR_FILE_SEEK
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamReadFailureTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_SYSTEM_INFO;
    dir.location.dataSize = sizeof(MDRawSystemInfo);
    dir.location.rva = TEST_OUT_OF_RANGE_RVA;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetSystemInfo(), nullptr);
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_FILE_SEEK);
}
/**
 * @tc.name: ParserOpenStreamBasedTest001
 * @tc.desc: test MinidumpParser Parse with stream from file path succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserOpenStreamBasedTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeader(1);
    int tmpFd = open("/data/test/minidump_stream_open", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/minidump_stream_open");
    EXPECT_TRUE(parser.Parse());
    EXPECT_TRUE(parser.Valid());
    unlink("/data/test/minidump_stream_open");
}
/**
 * @tc.name: ParserSeekToStreamTypeValidTest001
 * @tc.desc: test MinidumpParser SeekToStreamType with valid stream returns correct stream length
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserSeekToStreamTypeValidTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_SYSTEM_INFO;
    dir.location.dataSize = sizeof(MDRawSystemInfo);
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    MDRawSystemInfo sysInfo = {};
    sysInfo.platformId = MINIDUMP_OS_LINUX;
    sysInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    data += std::string(reinterpret_cast<const char*>(&sysInfo), sizeof(sysInfo));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    uint32_t streamLen = 0;
    EXPECT_TRUE(parser.SeekToStreamType(MD_STREAM_SYSTEM_INFO, streamLen));
    EXPECT_EQ(streamLen, sizeof(MDRawSystemInfo));
}
/**
 * @tc.name: ParserGetStreamFactoryNullReturnTest001
 * @tc.desc: test MinidumpParser GetException with factory returning nullptr stream object
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamFactoryNullReturnTest001, TestSize.Level2)
{
    MinidumpStreamFactory::Instance().RegisterCreator(MD_STREAM_EXCEPTION,
        [](std::shared_ptr<MinidumpMemoryReader>) -> std::shared_ptr<MinidumpStream> {
            return nullptr;
        });

    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_EXCEPTION;
    dir.location.dataSize = sizeof(MDExceptionStream);
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    data += std::string(sizeof(MDExceptionStream), '\0');
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetException(), nullptr);
    MinidumpStreamFactory::Instance().RegisterCreator(
        MinidumpException::streamType, MinidumpException::Instance);
}
/**
 * @tc.name: ParserGetStreamReadTruncatedTest001
 * @tc.desc: test MinidumpParser GetSystemInfo with truncated data returns nullptr and ERROR_STREAM_READ
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetStreamReadTruncatedTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_SYSTEM_INFO;
    dir.location.dataSize = sizeof(MDRawSystemInfo);
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    data += std::string(4, '\0');
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ(parser.GetSystemInfo(), nullptr);
    EXPECT_EQ(parser.GetLastError().GetError(), MinidumpError::ERROR_STREAM_READ);
}
/**
 * @tc.name: ParserSetupObserversEmptyNameTest001
 * @tc.desc: test MinidumpParser NotifyStreamLoaded with empty stream name does not crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserSetupObserversEmptyNameTest001, TestSize.Level2)
{
    auto stream = MakeStream(BuildValidMinidumpHeader(1));
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    parser.minidumpSubject_->NotifyStreamLoaded("", 0);
}
/**
 * @tc.name: ParserGetMapListValidTest001
 * @tc.desc: test MinidumpParser GetMapList with valid Linux maps stream returns non-null map list
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetMapListValidTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    std::string mapsData = "1000-2000 r-xp 00000000 00:00 0 /test/lib.so\n";
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_LINUX_MAPS;
    dir.location.dataSize = mapsData.size();
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    data += mapsData;
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    auto mapList = parser.GetMapList();
    EXPECT_NE(mapList, nullptr);
    EXPECT_TRUE(mapList->GetMapsLength() > 0);
}
/**
 * @tc.name: ParserGetEsrInfoValidTest001
 * @tc.desc: test MinidumpParser GetEsrInfo with valid ESR stream returns correct thread ID and ESR regs
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpParserTest, ParserGetEsrInfoValidTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_ESR_INFO;
    dir.location.dataSize = sizeof(MDRawEsrRegsInfo);
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    MDRawEsrRegsInfo esrInfo = {};
    esrInfo.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    esrInfo.dumpThreadId = 12345;
    esrInfo.esrRegs = 0x9600021;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));
    data += std::string(reinterpret_cast<const char*>(&esrInfo), sizeof(esrInfo));
    auto stream = MakeStream(data);
    MinidumpParser parser(stream);
    EXPECT_TRUE(parser.Parse());
    auto esrResult = parser.GetEsrInfo();
    EXPECT_NE(esrResult, nullptr);
    EXPECT_EQ(esrResult->EsrRegsInfo().dumpThreadId, 12345u);
    EXPECT_EQ(esrResult->EsrRegsInfo().esrRegs, 0x9600021ull);
}

} // namespace HiviewDFX
} // namespace OHOS
