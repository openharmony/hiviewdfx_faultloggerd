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

#include <fcntl.h>
#include <unistd.h>

#include "minidump_config.h"
#include "minidump_format.h"
#include "minidump_stream_pipeline.h"

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

constexpr mode_t TEST_FILE_PERMISSIONS = 0644;
constexpr uint32_t MINIDUMP_VERSION_TIMESTAMP = 0xa7900000;

class MinidumpStreamPipelineTest : public testing::Test {
public:
    void SetUp() override
    {
        auto& mgr = MinidumpConfigManager::Instance();
        mgr.SetConfig(MinidumpConfig());
        PerformanceOptimizer::Instance().Reset();
    }
    void TearDown() override
    {
        auto& mgr = MinidumpConfigManager::Instance();
        mgr.SetConfig(MinidumpConfig());
        PerformanceOptimizer::Instance().Reset();
    }
};

static std::string BuildValidMinidumpHeaderWithDir(uint32_t streamCount)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = streamCount;
    header.streamDirectoryRva = sizeof(MDRawHeader);
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

static std::string BuildMinidumpWithSystemInfo()
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
    return data;
}

/**
 * @tc.name: StreamPipelineConstructorTest001
 * @tc.desc: test StreamPipeline constructor with valid parser does not crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineConstructorTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeaderWithDir(1);
    int tmpFd = open("/data/test/stream_pipeline_ctor", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/stream_pipeline_ctor");
    EXPECT_TRUE(parser.Parse());
    {
        StreamPipeline pipeline(parser);
    }
    unlink("/data/test/stream_pipeline_ctor");
}

/**
 * @tc.name: StreamPipelineParseParallelEmptyPathTest001
 * @tc.desc: test StreamPipeline ParseParallel with empty path skips parallel parsing
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineParseParallelEmptyPathTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeaderWithDir(1);
    auto ss = MakeStream(data);
    MinidumpParser parser(ss);
    EXPECT_TRUE(parser.Parse());
    StreamPipeline pipeline(parser);
    pipeline.ParseParallel({MD_STREAM_SYSTEM_INFO});
}

/**
 * @tc.name: StreamPipelineParseParallelDisabledTest001
 * @tc.desc: test StreamPipeline ParseParallel with parallel parsing disabled skips parsing
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineParseParallelDisabledTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.enableParallelParsing = false;
    mgr.SetConfig(config);

    std::string data = BuildValidMinidumpHeaderWithDir(1);
    int tmpFd = open("/data/test/stream_pipeline_disabled", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/stream_pipeline_disabled");
    EXPECT_TRUE(parser.Parse());
    StreamPipeline pipeline(parser);
    pipeline.ParseParallel({MD_STREAM_SYSTEM_INFO});
    unlink("/data/test/stream_pipeline_disabled");
}

/**
 * @tc.name: StreamPipelineParseParallelValidTest001
 * @tc.desc: test StreamPipeline ParseParallel with valid system info stream injects stream
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineParseParallelValidTest001, TestSize.Level2)
{
    std::string data = BuildMinidumpWithSystemInfo();
    int tmpFd = open("/data/test/stream_parallel_valid", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/stream_parallel_valid");
    EXPECT_TRUE(parser.Parse());
    StreamPipeline pipeline(parser);
    pipeline.ParseParallel({MD_STREAM_SYSTEM_INFO});
    auto sysInfo = parser.GetSystemInfo();
    EXPECT_NE(sysInfo, nullptr);
    unlink("/data/test/stream_parallel_valid");
}

/**
 * @tc.name: StreamPipelineParseParallelMissingStreamTest001
 * @tc.desc: test StreamPipeline ParseParallel with missing stream type does not crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineParseParallelMissingStreamTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeaderWithDir(1);
    int tmpFd = open("/data/test/stream_parallel_missing", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/stream_parallel_missing");
    EXPECT_TRUE(parser.Parse());
    StreamPipeline pipeline(parser);
    pipeline.ParseParallel({MD_STREAM_SYSTEM_INFO, MD_STREAM_MEMORY_LIST});
    EXPECT_EQ(parser.GetSystemInfo(), nullptr);
    unlink("/data/test/stream_parallel_missing");
}

/**
 * @tc.name: StreamPipelineParseParallelExtentOutOfBoundsTest001
 * @tc.desc: test StreamPipeline ParseParallel with declared size exceeding file does not inject stream
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineParseParallelExtentOutOfBoundsTest001, TestSize.Level2)
{
    MDRawHeader header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | MINIDUMP_VERSION_TIMESTAMP;
    header.numberOfStreams = 1;
    header.streamDirectoryRva = sizeof(MDRawHeader);
    MDRawDirectory dir = {};
    dir.streamType = MD_STREAM_SYSTEM_INFO;
    dir.location.rva = sizeof(MDRawHeader) + sizeof(MDRawDirectory);
    dir.location.dataSize = sizeof(MDRawSystemInfo) + 1;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dir), sizeof(dir));

    int tmpFd = open("/data/test/stream_parallel_extent_oob", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/stream_parallel_extent_oob");
    EXPECT_TRUE(parser.Parse());
    StreamPipeline pipeline(parser);
    pipeline.ParseParallel({MD_STREAM_SYSTEM_INFO});
    EXPECT_EQ(parser.GetSystemInfo(), nullptr);
    unlink("/data/test/stream_parallel_extent_oob");
}

/**
 * @tc.name: StreamPipelineParseParallelEmptyListTest001
 * @tc.desc: test StreamPipeline ParseParallel with empty stream type list does not crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamPipelineTest, StreamPipelineParseParallelEmptyListTest001, TestSize.Level2)
{
    std::string data = BuildValidMinidumpHeaderWithDir(1);
    int tmpFd = open("/data/test/stream_parallel_empty_list", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, data.c_str(), data.size());
    close(tmpFd);
    MinidumpParser parser("/data/test/stream_parallel_empty_list");
    EXPECT_TRUE(parser.Parse());
    StreamPipeline pipeline(parser);
    pipeline.ParseParallel({});
    unlink("/data/test/stream_parallel_empty_list");
}

} // namespace HiviewDFX
} // namespace OHOS
