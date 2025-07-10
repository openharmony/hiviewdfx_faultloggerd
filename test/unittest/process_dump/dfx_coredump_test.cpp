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

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "dfx_coredump_service.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
MAYBE_UNUSED constexpr const char* const TEST_TEMP_FILE = "/data/test/testfile";
class DfxCoreDumpTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {}
    void SetUp() {};
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: DfxCoreDumpTest001
 * @tc.desc: test coredump set pid, tid, vmpid
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest001: start.";
    pid_t vmPid = 666; // 666
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    coreDumpService.SetVmPid(vmPid);
    auto coreDumpThread = coreDumpService.GetCoreDumpThread();
    ASSERT_EQ(coreDumpThread.targetPid, pid);
    ASSERT_EQ(coreDumpThread.targetTid, tid);
    ASSERT_EQ(coreDumpThread.vmPid, vmPid);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest001: end.";
}

/**
 * @tc.name: DfxCoreDumpTest002
 * @tc.desc: test coredump StartCoreDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest002: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.StartCoreDump();
    ASSERT_TRUE(!ret);
    coreDumpService.status_ = OHOS::HiviewDFX::CoreDumpService::WriteStatus::WRITE_SEGMENT_HEADER_STAGE;
    ret = coreDumpService.StartCoreDump();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest002: end.";
}

/**
 * @tc.name: DfxCoreDumpTest003
 * @tc.desc: test coredump GetBundleNameItem function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest003: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    auto bundleName = coreDumpService.GetBundleNameItem();
    ASSERT_TRUE(bundleName.empty());
    GTEST_LOG_(INFO) << "DfxCoreDumpTest003: end.";
}

/**
 * @tc.name: DfxCoreDumpTest004
 * @tc.desc: test coredump CreateFileForCoreDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest004: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    auto fd = coreDumpService.CreateFileForCoreDump();
    ASSERT_EQ(fd, -1);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest004: end.";
}

/**
 * @tc.name: DfxCoreDumpTest005
 * @tc.desc: test coredump CreateFile function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest005: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.CreateFile();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest005: end.";
}

/**
 * @tc.name: DfxCoreDumpTest006
 * @tc.desc: test coredump ObtainDumpRegion function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest006: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    std::string line = "5a0eb08000-5a0eb09000 r-xp 00007000 00:00 0            /system/lib/test.z.so";
    DumpMemoryRegions region;
    coreDumpService.ObtainDumpRegion(line, region);
    ASSERT_EQ(region.memorySizeHex, 0x1000);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest006: end.";
}

/**
 * @tc.name: DfxCoreDumpTest007
 * @tc.desc: test coredump when isHwasanHap_ is true
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest007: start.";
    CoreDumpService coreDumpService = CoreDumpService(getpid(), gettid(), DfxRegs::Create());
    coreDumpService.isHwasanHap_ = true;
    ASSERT_FALSE(coreDumpService.IsDoCoredump());
    GTEST_LOG_(INFO) << "DfxCoreDumpTest007: end.";
}

/**
 * @tc.name: DfxCoreDumpTest008
 * @tc.desc: test coredump get corefilesize when pid is 0
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest008: start.";
    CoreDumpService coreDumpService = CoreDumpService(0, 0, DfxRegs::Create());
    uint64_t coreFileSize = coreDumpService.GetCoreFileSize(0);
    ASSERT_EQ(coreFileSize, 0);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest008: end.";
}

/**
 * @tc.name: DfxCoreDumpTest009
 * @tc.desc: test coredump createfile when pid is valid
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest009: start.";
    CoreDumpService coreDumpService = CoreDumpService(0, 0, DfxRegs::Create());
    bool ret = coreDumpService.CreateFile();
    ASSERT_TRUE(!ret);
    coreDumpService.coreDumpThread_.targetPid = 99999; // 99999 invalid pid
    ret = coreDumpService.CreateFile();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest009: end.";
}

/**
 * @tc.name: DfxCoreDumpTest010
 * @tc.desc: test coredump MmapForFd function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest010: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.MmapForFd();
    ASSERT_TRUE(!ret);
    coreDumpService.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_TRUE(coreDumpService.fd_ > 0);
    ret = coreDumpService.MmapForFd();
    ASSERT_TRUE(!ret);
    coreDumpService.coreFileSize_ = 1024 * 1024;
    ret = coreDumpService.MmapForFd();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest010: end.";
}

/**
 * @tc.name: DfxCoreDumpTest011
 * @tc.desc: test coredump WriteSegmentHeader function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest011: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.WriteSegmentHeader();
    ASSERT_TRUE(!ret);

    coreDumpService.status_ = OHOS::HiviewDFX::CoreDumpService::WriteStatus::WRITE_SEGMENT_HEADER_STAGE;
    coreDumpService.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    coreDumpService.coreFileSize_ = 1024 * 1024;
    ret = coreDumpService.MmapForFd();
    ASSERT_TRUE(ret);
    DumpMemoryRegions dummyRegion {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    coreDumpService.maps_ = { dummyRegion };

    ret = coreDumpService.WriteSegmentHeader();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest011: end.";
}

/**
 * @tc.name: DfxCoreDumpTest012
 * @tc.desc: test coredump WriteNoteSegment function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest012: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(0, 0, DfxRegs::Create());
    bool ret = coreDumpService.WriteNoteSegment();
    ASSERT_TRUE(!ret);
    coreDumpService.status_ = OHOS::HiviewDFX::CoreDumpService::WriteStatus::WRITE_NOTE_SEGMENT_STAGE;
    ret = coreDumpService.WriteNoteSegment();
    ASSERT_TRUE(!ret);

    coreDumpService.coreDumpThread_.targetPid = pid;
    coreDumpService.coreDumpThread_.targetTid = tid;
    coreDumpService.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    coreDumpService.coreFileSize_ = 1024 * 1024;
    ret = coreDumpService.MmapForFd();
    ASSERT_TRUE(ret);
    DumpMemoryRegions dummyRegion {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    coreDumpService.maps_ = { dummyRegion };
    coreDumpService.currentPointer_ = coreDumpService.mappedMemory_;

    ret = coreDumpService.WriteNoteSegment();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest012: end.";
}

/**
 * @tc.name: DfxCoreDumpTest013
 * @tc.desc: test coredump WriteLoadSegment function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest013: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.WriteLoadSegment();
    ASSERT_TRUE(!ret);

    coreDumpService.status_ = OHOS::HiviewDFX::CoreDumpService::WriteStatus::WRITE_LOAD_SEGMENT_STAGE;
    ret = coreDumpService.WriteNoteSegment();
    ASSERT_TRUE(!ret);

    coreDumpService.SetVmPid(getpid());
    coreDumpService.fd_ = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    coreDumpService.coreFileSize_ = 1024 * 1024;
    ret = coreDumpService.MmapForFd();
    ASSERT_TRUE(ret);

    constexpr size_t kRegionSize = 64;
    auto* sourceBuffer = static_cast<char*>(malloc(kRegionSize));
    (void)memset_s(sourceBuffer, kRegionSize, 0xA5, kRegionSize);
    uintptr_t fakeVaddr = reinterpret_cast<uintptr_t>(sourceBuffer);

    coreDumpService.currentPointer_ = coreDumpService.mappedMemory_ + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    Elf64_Phdr* ptLoad = reinterpret_cast<Elf64_Phdr*>(coreDumpService.mappedMemory_ +
        sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr));

    ptLoad->p_vaddr = fakeVaddr;
    ptLoad->p_memsz = kRegionSize;

    coreDumpService.ePhnum_ = 2;

    ret = coreDumpService.WriteLoadSegment();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest013: end.";
}

/**
 * @tc.name: DfxCoreDumpTest014
 * @tc.desc: test coredump WriteSectionHeader function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest014: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.WriteSectionHeader();
    ASSERT_TRUE(!ret);

    coreDumpService.status_ = OHOS::HiviewDFX::CoreDumpService::WriteStatus::WRITE_SECTION_HEADER_STAGE;

    ret = coreDumpService.WriteSectionHeader();
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest014: end.";
}

/**
 * @tc.name: DfxCoreDumpTest015
 * @tc.desc: test coredump FinishCoreDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest015: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.FinishCoreDump();
    ASSERT_TRUE(!ret);
    coreDumpService.status_ = OHOS::HiviewDFX::CoreDumpService::WriteStatus::STOP_STAGE;
    ret = coreDumpService.FinishCoreDump();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest015: end.";
}

/**
 * @tc.name: DfxCoreDumpTest016
 * @tc.desc: test coredump StartFirstStageDump and StartSecondStageDump function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest016: start.";
    pid_t forkPid = fork();
    if (forkPid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new process";
    } else if (forkPid == 0) {
        GTEST_LOG_(INFO) << "fork success";
        auto pid = getpid();
        auto tid = gettid();
        ProcessDumpRequest request;
        CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
        coreDumpService.StartFirstStageDump(request);
        coreDumpService.StartSecondStageDump(pid, request);
        exit(0);
    }
    int status;
    bool isSuccess = waitpid(forkPid, &status, 0) != -1;
    ASSERT_TRUE(isSuccess);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest016: end.";
}

/**
 * @tc.name: DfxCoreDumpTest017
 * @tc.desc: test coredump IsCoredumpSignal and IsHwasanCoredumpEnabled function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest017: start.";
    ProcessDumpRequest request;
    bool ret = CoreDumpService::IsCoredumpSignal(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 42;
    ret = CoreDumpService::IsCoredumpSignal(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 0;
    request.siginfo.si_code = 3;
    ret = CoreDumpService::IsCoredumpSignal(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 3;
    ret = CoreDumpService::IsCoredumpSignal(request);
    ASSERT_TRUE(ret);

    ret = CoreDumpService::IsHwasanCoredumpEnabled();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest017: end.";
}

/**
 * @tc.name: DfxCoreDumpTest018
 * @tc.desc: test coredump VerifyTrustlist function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest018: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.VerifyTrustlist();
    ASSERT_TRUE(!ret);
    coreDumpService.bundleName_ = "test.hap";
    ret = coreDumpService.VerifyTrustlist();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest018: end.";
}

/**
 * @tc.name: DfxCoreDumpTest019
 * @tc.desc: test coredump IsCoredumpAllowed function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest019: start.";
    ProcessDumpRequest request;
    bool ret = CoreDumpService::IsCoredumpAllowed(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 3;
    ret = CoreDumpService::IsCoredumpAllowed(request);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest019: end.";
}

/**
 * @tc.name: DfxCoreDumpTest020
 * @tc.desc: test coredump GetKeyThreadData function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest020: start.";
    ProcessDumpRequest request;
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid, DfxRegs::Create());
    bool ret = coreDumpService.GetKeyThreadData(request);
    ASSERT_TRUE(!ret);
    request.tid = tid;
    ret = coreDumpService.GetKeyThreadData(request);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest020: end.";
}

/**
 * @tc.name: DfxCoreDumpTest021
 * @tc.desc: test coredump GetCoredumpFileName function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest021: start.";
    CoreDumpService coreDumpService = CoreDumpService();
    std::string fileName = coreDumpService.GetCoredumpFileName();
    ASSERT_TRUE(fileName.empty());
    coreDumpService.bundleName_ = "test.hap";
    fileName = coreDumpService.GetCoredumpFileName();
    ASSERT_TRUE(!fileName.empty());
    ASSERT_EQ(fileName, "test.hap.dmp");
    GTEST_LOG_(INFO) << "DfxCoreDumpTest021: end.";
}

/**
 * @tc.name: DfxCoreDumpTest022
 * @tc.desc: test coredump GetCoredumpFilePath function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest022: start.";
    CoreDumpService coreDumpService = CoreDumpService();
    std::string filePath = coreDumpService.GetCoredumpFilePath();
    ASSERT_TRUE(filePath.empty());
    coreDumpService.bundleName_ = "test.hap";
    filePath = coreDumpService.GetCoredumpFilePath();
    ASSERT_TRUE(!filePath.empty());
    ASSERT_EQ(filePath, "/data/storage/el2/base/files/test.hap.dmp");
    GTEST_LOG_(INFO) << "DfxCoreDumpTest022: end.";
}

/**
 * @tc.name: DfxCoreDumpTest023
 * @tc.desc: test coredump UnlinkFile function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest023: start.";
    auto ret = CoreDumpService::UnlinkFile("");
    ASSERT_TRUE(!ret);
    std::ofstream outfile(TEST_TEMP_FILE);
    if (!outfile) {
        GTEST_LOG_(ERROR) << "Failed to open file";
    }
    outfile << "testdata" << std::endl;
    outfile.close();
    ret = CoreDumpService::UnlinkFile(TEST_TEMP_FILE);
    ASSERT_TRUE(ret);
    ret = CoreDumpService::UnlinkFile(TEST_TEMP_FILE);
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest023: end.";
}

/**
 * @tc.name: DfxCoreDumpTest024
 * @tc.desc: test coredump UnlinkFile function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpTest024: start.";
    auto ret = CoreDumpService::AdjustFileSize(-1, 0);
    ASSERT_TRUE(!ret);
    int fd = open(TEST_TEMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_TRUE(fd > 0);
    ret = CoreDumpService::AdjustFileSize(fd, 0);
    ASSERT_TRUE(!ret);
    ret = CoreDumpService::AdjustFileSize(fd, 1024);
    ASSERT_TRUE(ret);
    close(fd);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest024: end.";
}
}
