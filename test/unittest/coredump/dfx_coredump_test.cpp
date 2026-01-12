/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <unistd.h>
#include <vector>
#include <fstream>
#include <iostream>

#include "coredump_dump_generator.h"
#include "coredump_file_manager.h"
#include "coredump_generator_factory.h"
#define private public
#include "coredump_mapping_manager.h"
#include "coredump_manager.h"

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
constexpr const char* const TEST_TEMP_FILE = "/data/test/testfile";
class DfxCoreDumpTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: FullCoredumpGenerator001
 * @tc.desc: test coredump set pid, tid, vmpid
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator001: start.";
    ProcessDumpRequest request;
    request.pid = getpid();
    request.tid = gettid();
    FullCoredumpGenerator generator(request);
    ASSERT_EQ(generator.coredumpProc_.targetPid, getpid());
    ASSERT_EQ(generator.coredumpProc_.keyTid, gettid());
    GTEST_LOG_(INFO) << "FullCoredumpGenerator001: end.";
}

/**
 * @tc.name: FullCoredumpGenerator002
 * @tc.desc: test coredump StartCoreDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator002: start.";
    ProcessDumpRequest request;
    request.pid = getpid();
    request.tid = gettid();
    FullCoredumpGenerator generator(request);
    bool ret = generator.TriggerCoredump();
    EXPECT_FALSE(ret); // native not support coredump

    GTEST_LOG_(INFO) << "FullCoredumpGenerator002: end.";
}

/**
 * @tc.name: FullCoredumpGenerator003
 * @tc.desc: test coredump CreateFile function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator003: start.";
    ProcessDumpRequest req;
    req.pid = getpid();
    req.tid = gettid();
    FullCoredumpGenerator generator(req);

    bool ret = generator.MmapCoredumpFile();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "FullCoredumpGenerator003: end.";
}

/**
 * @tc.name: FullCoredumpGenerator004
 * @tc.desc: test coredump WriteSegmentHeader function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator004: start.";
    ProcessDumpRequest request;
    request.pid = getpid();
    request.tid = gettid();
    FullCoredumpGenerator generator(request);
    bool ret = generator.WriteSegmentHeader();
    ASSERT_TRUE(!ret);

    CoredumpFileManager fileManager;
    fileManager.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    fileManager.coreFileSize_ = 1024 * 1024;
    fileManager.MmapForFd();
    generator.bw_ = make_unique<CoredumpBufferWriter>(fileManager.GetMappedMemory(), fileManager.GetCoreFileSize());
    DumpMemoryRegions dummyRegion {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    auto& mappingManager = CoredumpMappingManager::GetInstance();
    mappingManager.maps_ = { dummyRegion };

    ret = generator.WriteSegmentHeader();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "FullCoredumpGenerator004: end.";
}

/**
 * @tc.name: FullCoredumpGenerator005
 * @tc.desc: test coredump WriteNoteSegment function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator005: start.";
    ProcessDumpRequest request;
    request.pid = getpid();
    request.tid = gettid();
    FullCoredumpGenerator generator(request);;
    bool ret = generator.WriteNoteSegment();
    EXPECT_FALSE(ret);


    CoredumpFileManager fileManager;
    fileManager.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    fileManager.coreFileSize_ = 1024 * 1024;
    fileManager.MmapForFd();
    generator.bw_ =
        std::make_unique<CoredumpBufferWriter>(fileManager.GetMappedMemory(), fileManager.GetCoreFileSize());

    generator.coredumpProc_.targetPid = getpid();
    generator.coredumpProc_.keyTid = 0;
    ret = generator.WriteNoteSegment();
    EXPECT_FALSE(ret);
    generator.coredumpProc_.keyTid = gettid();
    DumpMemoryRegions dummyRegion {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    auto& mappingManager = CoredumpMappingManager::GetInstance();
    mappingManager.maps_ = { dummyRegion };

    ret = generator.WriteNoteSegment();
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << "FullCoredumpGenerator005: end.";
}

/**
 * @tc.name: FullCoredumpGenerator006
 * @tc.desc: test coredump WriteLoadSegment function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator006: start.";
    ProcessDumpRequest request;
    request.pid = getpid();
    request.tid = gettid();
    FullCoredumpGenerator generator(request);;

    bool ret = generator.WriteLoadSegment(0);
    ASSERT_TRUE(!ret);

    ret = generator.WriteLoadSegment(getpid());
    ASSERT_TRUE(!ret);

    ret = generator.WriteNoteSegment();
    ASSERT_TRUE(!ret);

    CoredumpFileManager fileManager;
    fileManager.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    fileManager.coreFileSize_ = 1024 * 1024;
    fileManager.MmapForFd();
    generator.bw_ =
        std::make_unique<CoredumpBufferWriter>(fileManager.GetMappedMemory(), fileManager.GetCoreFileSize());
    generator.bw_->Advance(sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr));
    generator.ePhnum_ = 2;
    ret = generator.WriteLoadSegment(getpid());
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "FullCoredumpGenerator006: end.";
}

/**
 * @tc.name: FullCoredumpGenerator007
 * @tc.desc: test coredump WriteSectionHeader function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator007: start.";
    ProcessDumpRequest request;
    request.pid = getpid();
    request.tid = gettid();
    FullCoredumpGenerator generator(request);;
    CoredumpFileManager fileManager;

    bool ret = generator.WriteSectionHeader();
    EXPECT_FALSE(ret);

    fileManager.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    fileManager.coreFileSize_ = 1024 * 1024;
    fileManager.MmapForFd();
    generator.bw_ =
        std::make_unique<CoredumpBufferWriter>(fileManager.GetMappedMemory(), fileManager.GetCoreFileSize());
    ret = generator.WriteSectionHeader();
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << "FullCoredumpGenerator007: end.";
}

/**
 * @tc.name: FullCoredumpGenerator008
 * @tc.desc: test coredump FinishCoreDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator008: start.";
    pid_t pid = fork();
    if (pid == 0) {
        ProcessDumpRequest request;
        FullCoredumpGenerator generator(request);
        generator.FinishCoredump(false);
        exit(0);
    } else {
        ASSERT_TRUE(pid > 0);
        waitpid(pid, nullptr, 0);
        GTEST_LOG_(INFO) << "FullCoredumpGenerator008: end.";
    }
}

/**
 * @tc.name: FullCoredumpGenerator009
 * @tc.desc: test coredump StartFirstStageDump and StartSecondStageDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, FullCoredumpGenerator009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FullCoredumpGenerator009: start.";
    pid_t forkPid = fork();
    if (forkPid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new process";
    } else if (forkPid == 0) {
        GTEST_LOG_(INFO) << "fork success";
        auto pid = getpid();
        ProcessDumpRequest request;
        std::vector<char> buf(1024 * 1024);
        FullCoredumpGenerator generator(request);
        generator.bw_ = std::make_unique<CoredumpBufferWriter>(buf.data(), buf.size());
        generator.TriggerCoredump();
        generator.WriteElfHeader();
        generator.DumpMemoryForPid(pid);
        exit(0);
    }
    int status;
    bool isSuccess = waitpid(forkPid, &status, 0) != -1;
    ASSERT_TRUE(isSuccess);
    GTEST_LOG_(INFO) << "FullCoredumpGenerator009: end.";
}

/**
 * @tc.name: HwAsanDumpGenerator001
 * @tc.desc: test coredump StartFirstStageDump and StartSecondStageDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, HwAsanDumpGenerator001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HwAsanDumpGenerator001: start.";
    ProcessDumpRequest request;
    HwAsanDumpGenerator generator(request);
    bool ret = generator.LoadConfig();
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "HwAsanDumpGenerator001: end.";
}

/**
 * @tc.name: CoredumpFileManager001
 * @tc.desc: test coredump GetCoredumpFileName function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager001: start.";

    CoredumpFileManager fileManager;
    std::string fileName = fileManager.GetCoredumpFileName();
    ASSERT_TRUE(fileName.empty());
    fileManager.bundleName_ = "test.hap";
    fileName = fileManager.GetCoredumpFileName();
    ASSERT_TRUE(!fileName.empty());
    ASSERT_EQ(fileName, "test.hap.dmp");
    GTEST_LOG_(INFO) << "CoredumpFileManager001: end.";
}

/**
 * @tc.name: CoredumpFileManager002
 * @tc.desc: test coredump GetCoredumpFilePath function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager002: start.";
    CoredumpFileManager fileManager;

    std::string filePath = fileManager.GetCoredumpFilePath();
    ASSERT_TRUE(filePath.empty());
    fileManager.bundleName_ = "test.hap";
    filePath = fileManager.GetCoredumpFilePath();
    ASSERT_TRUE(!filePath.empty());
    ASSERT_EQ(filePath, "/data/storage/el2/base/files/test.hap.dmp");
    GTEST_LOG_(INFO) << "CoredumpFileManager002: end.";
}

/**
 * @tc.name: CoredumpFileManager003
 * @tc.desc: test coredump UnlinkFile function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager003: start.";
    auto ret = CoredumpFileManager::UnlinkFile("");
    ASSERT_TRUE(!ret);
    std::ofstream outfile(TEST_TEMP_FILE);
    if (!outfile) {
        GTEST_LOG_(ERROR) << "Failed to open file";
    }
    outfile << "testdata" << std::endl;
    outfile.close();
    ret = CoredumpFileManager::UnlinkFile(TEST_TEMP_FILE);
    ASSERT_TRUE(ret);
    ret = CoredumpFileManager::UnlinkFile(TEST_TEMP_FILE);
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "CoredumpFileManager003: end.";
}

/**
 * @tc.name: CoredumpFileManager004
 * @tc.desc: test coredump UnlinkFile function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager004: start.";
    CoredumpFileManager fileManager;
    fileManager.fd_ = -1;
    auto ret = fileManager.AdjustFileSize(0);
    ASSERT_TRUE(!ret);
    int fd = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_TRUE(fd > 0);
    fileManager.fd_ = fd;
    ret = fileManager.AdjustFileSize(0);
    ASSERT_TRUE(!ret);
    ret = fileManager.AdjustFileSize(1024);
    ASSERT_TRUE(ret);
    close(fd);
    GTEST_LOG_(INFO) << "CoredumpFileManager004: end.";
}

/**
 * @tc.name: CoredumpFileManager005
 * @tc.desc: test coredump CreateFileForCoreDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager005: start.";
    CoredumpFileManager fileManager;
    auto ret = fileManager.CreateFileForCoreDump();
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpFileManager005: end.";
}

/**
 * @tc.name: CoredumpFileManager006
 * @tc.desc: test coredump get corefilesize when pid is 0
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager006: start.";
    CoredumpFileManager fileManager;
    uint64_t coreFileSize = fileManager.GetCoreFileSize();
    ASSERT_EQ(coreFileSize, 0);
    GTEST_LOG_(INFO) << "CoredumpFileManager006: end.";
}

/**
 * @tc.name: CoredumpFileManager007
 * @tc.desc: test coredump createfile when pid is valid
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager007: start.";
    CoredumpFileManager fileManager;
    fileManager.targetPid_ = 0;
    bool ret = fileManager.CreateFile();
    ASSERT_TRUE(!ret);
    fileManager.targetPid_ = 99999; // 99999 invalid pid
    ret = fileManager.CreateFile();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "CoredumpFileManager007: end.";
}

/**
 * @tc.name: CoredumpFileManager008
 * @tc.desc: test coredump MmapForFd function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager008: start.";
    CoredumpFileManager fileManager;
    bool ret = fileManager.MmapForFd();
    ASSERT_TRUE(!ret);
    fileManager.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_TRUE(fileManager.fd_ > 0);
    ret = fileManager.MmapForFd();
    ASSERT_TRUE(!ret);
    fileManager.coreFileSize_ = 1024 * 1024;
    ret = fileManager.MmapForFd();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "CoredumpFileManager008: end.";
}

/**
 * @tc.name: CoredumpFileManager009
 * @tc.desc: test coredump MmapForFd function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpFileManager009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFileManager009: start.";
    CoredumpFileManager fileManager;
    string path = "test.dmp";
    auto ret = fileManager.RegisterCancelCoredump(path);
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << "CoredumpFileManager009: end.";
}

/**
 * @tc.name: CoredumpGeneratorFactory001
 * @tc.desc: test coredump MmapForFd function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpGeneratorFactory001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpGeneratorFactory001: start.";

    ProcessDumpRequest request;
    CoredumpGeneratorFactory coredumpGeneratorFactory;
    auto ret = coredumpGeneratorFactory.CreateGenerator(request);
    EXPECT_TRUE(!ret);

    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 3;
    ret = coredumpGeneratorFactory.CreateGenerator(request);
    EXPECT_TRUE(ret);

    request.siginfo.si_signo = 6;
    request.siginfo.si_code = 3;
    ret = coredumpGeneratorFactory.CreateGenerator(request);
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpGeneratorFactory001: end.";
}

/**
 * @tc.name: CoredumpManager001
 * @tc.desc: test coredump MmapForFd function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, CoredumpManager001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpManager001: start.";
    CoredumpManager manager;
    ProcessDumpRequest request;
    request.pid = getpid();
    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 3;
    manager.ProcessRequest(request);
    manager.TriggerCoredump();
    manager.DumpMemoryForPid(-1);
    EXPECT_NE(manager.generator_, nullptr);
    request.siginfo.si_signo = 38;
    manager.ProcessRequest(request);
    manager.TriggerCoredump();
    manager.DumpMemoryForPid(getpid());
    EXPECT_EQ(manager.generator_, nullptr);
    GTEST_LOG_(INFO) << "CoredumpManager001: end.";
}
}
