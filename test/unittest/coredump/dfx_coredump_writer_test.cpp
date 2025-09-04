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

#include "dfx_util.h"

#include "coredump_header_writer.h"
#include "coredump_note_segment_writer.h"
#include "coredump_mapping_manager.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxCoreDumpWriterTest : public testing::Test {
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
 * @tc.name: DfxCoreDumpWriterTest001
 * @tc.desc: test ProgramSegmentHeaderWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, DfxCoreDumpWriterTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest001: start.";
    constexpr size_t bufferSize = 4096;
    std::vector<char> buffer(bufferSize);
    auto bw = std::make_unique<CoredumpBufferWriter>(buffer.data(), buffer.size());
    DumpMemoryRegions dummyRegion1 {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    (void)strcpy_s(dummyRegion1.priority, 5, "r-xp");

    DumpMemoryRegions dummyRegion2 {
        .startHex = 0x3000,
        .endHex = 0x4000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    (void)strcpy_s(dummyRegion2.priority, 5, "rw-p");

    auto& instance = CoredumpMappingManager::GetInstance();
    std::string line = "5a0eb08000-5a0eb09000 r-xp 00007000 00:00 0            /system/lib/test.z.so";
    DumpMemoryRegions region;
    instance.ObtainDumpRegion(line, region);

    std::vector<DumpMemoryRegions> maps = { dummyRegion1, dummyRegion2, region};
    ProgramSegmentHeaderWriter writer(maps, *bw);

    bool ret = writer.Write();

    ASSERT_TRUE(ret);
    ASSERT_TRUE(writer.GetPhnum() > 0);

    Elf64_Phdr phdr;
    writer.PtLoadHeaderFill(phdr, region);
    region.priority[0] = 'r';
    writer.PtLoadHeaderFill(phdr, region);
    region.priority[0] = '-';
    writer.PtLoadHeaderFill(phdr, region);
    region.priority[0] = 'f';
    writer.PtLoadHeaderFill(phdr, region);

    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest001: end.";
}

/**
 * @tc.name: DfxCoreDumpWriterTest003
 * @tc.desc: test SectionHeaderTableWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, DfxCoreDumpWriterTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest003: start.";
    constexpr size_t bufferSize = 4096;
    std::vector<char> buffer(bufferSize, 0);

    auto bufferWriter = std::make_unique<CoredumpBufferWriter>(buffer.data(), buffer.size());
    SectionHeaderTableWriter writer(*bufferWriter);
    bool result = writer.Write();

    writer.AdjustOffset(10);
    writer.AdjustOffset(0);

    Elf64_Phdr phdr;
    Elf64_Xword shFlag;

    phdr.p_flags = 0;
    writer.SetShFlag(&phdr, shFlag);
    phdr.p_flags = PF_R;
    writer.SetShFlag(&phdr, shFlag);
    phdr.p_flags = (PF_R | PF_W);
    writer.SetShFlag(&phdr, shFlag);
    phdr.p_flags = (PF_R | PF_X);
    writer.SetShFlag(&phdr, shFlag);

    ASSERT_TRUE(result);
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest003: end.";
}

/**
 * @tc.name: NoteSegmentWriter001
 * @tc.desc: test NoteSegmentWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, NoteSegmentWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "NoteSegmentWriter001: start.";
    constexpr size_t bufferSize = 4096;
    std::vector<char> buffer(bufferSize, 0);

    auto bw = std::make_unique<OHOS::HiviewDFX::CoredumpBufferWriter>(buffer.data(), buffer.size());

    CoredumpProc coredumpProc;
    coredumpProc.targetPid = getpid();
    coredumpProc.keyTid = gettid();
    DumpMemoryRegions dummyRegion {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    std::vector<DumpMemoryRegions> maps = { dummyRegion };

    NoteSegmentWriter writer(coredumpProc, maps, *bw);
    std::shared_ptr<DfxRegs> regs = DfxRegs::Create();
    CoredumpThread keyThread;
    keyThread.keyRegs_ = regs;
    writer.SetKeyThreadData(keyThread);
    bool result = writer.Write();
    ASSERT_TRUE(result);

    writer.cfg_.process.prpsinfo = false;
    writer.cfg_.process.multiThread = false;
    writer.cfg_.process.auxv = false;
    writer.cfg_.process.dumpMappings = false;
    writer.InitWriters();

    GTEST_LOG_(INFO) << "NoteSegmentWriter001: end.";
}

/**
 * @tc.name: FileRegionWriter001
 * @tc.desc: test NoteSegmentWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, FileRegionWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FileRegionWriter001: start.";
    std::vector<char> buf(1024 * 1024);
    CoredumpBufferWriter bw(buf.data(), buf.size());

    auto& mappingManager = CoredumpMappingManager::GetInstance();
    mappingManager.Parse(getpid());
    mappingManager.EstimateFileSize();
    auto maps = mappingManager.GetMaps();
    FileRegionWriter writer(maps, bw);

    bool ret = writer.WriteAddrRelated();
    EXPECT_TRUE(ret);

    Elf64_Half lineNumber = 0;
    ret = writer.WriteFilePath(lineNumber);
    EXPECT_TRUE(ret);

    bw.Reset();
    bw.capacity_ = 10;

    ret = writer.WriteAddrRelated();
    EXPECT_FALSE(ret);

    ret = writer.WriteFilePath(lineNumber);
    EXPECT_FALSE(ret);

    std::vector<DumpMemoryRegions> emptyMaps;
    FileRegionWriter writer2(emptyMaps, bw);
    writer2.WriteAddrRelated();
    writer2.WriteFilePath(lineNumber);

    GTEST_LOG_(INFO) << "FileRegionWriter001: end.";
}

/**
 * @tc.name: ThreadNoteWriter001
 * @tc.desc: test ThreadNoteWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, ThreadNoteWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadNoteWriter001: start.";

    std::vector<char> buf(1014);
    CoredumpBufferWriter bw(buf.data(), 1);
    ThreadNoteWriter writer(getpid(), getpid(), bw);
    auto ret = writer.ArmPacMaskWrite(getpid());
    EXPECT_FALSE(ret);
    bw.capacity_ = sizeof(UserPacMask) + 1;
    bw.Reset();
    ret = writer.ArmPacMaskWrite(getpid());
    EXPECT_FALSE(ret);

    bw.capacity_ = sizeof(prstatus_t) + 1;
    bw.Reset();
    ret = writer.PrstatusWrite(getpid());
    EXPECT_FALSE(ret);

    bw.capacity_ = sizeof(user_fpsimd_struct) + 1;
    bw.Reset();
    ret = writer.FpregsetWrite(getpid());
    EXPECT_FALSE(ret);

    bw.capacity_ = sizeof(siginfo_t) + 1;
    bw.Reset();
    writer.SiginfoWrite(getpid());
    EXPECT_FALSE(ret);

    writer.ArmTaggedAddrCtrlWrite();
    GTEST_LOG_(INFO) << "ThreadNoteWriter001: end.";
}

/**
 * @tc.name: ThreadNoteWriter002
 * @tc.desc: test ThreadNoteWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, ThreadNoteWriter002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadNoteWriter002: start.";

    std::vector<char> buf(1024);
    CoredumpBufferWriter bw(buf.data(), buf.size());
    ThreadNoteWriter writer(getpid(), getpid(), bw);
    prstatus_t prStatus;
    auto ret = writer.GetPrStatus(prStatus, -1);
    EXPECT_FALSE(ret);

    GTEST_LOG_(INFO) << "ThreadNoteWriter002: end.";
}

/**
 * @tc.name: MultiThreadNoteWriter001
 * @tc.desc: test MultiThreadNoteWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, MultiThreadNoteWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MultiThreadNoteWriter001: start.";
        std::vector<char> buf(1024);
    CoredumpBufferWriter bw(buf.data(), buf.size());

    MultiThreadNoteWriter writer(-1, gettid(), bw, {});
    bool ret = writer.Write();
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "MultiThreadNoteWriter001: end.";
}

/**
 * @tc.name: PrpsinfoWriter001
 * @tc.desc: test PrpsinfoWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, PrpsinfoWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "PrpsinfoWriter001: start.";
    std::vector<char> buf(1);
    CoredumpBufferWriter bw(buf.data(), buf.size());

    PrpsinfoWriter writer(getpid(), bw);

    bool ret = writer.Write();
    EXPECT_FALSE(ret);

    prpsinfo_t info;
    ret = writer.ReadProcessStat(info);
    EXPECT_TRUE(ret);

    ret = writer.ReadProcessStatus(info);
    EXPECT_TRUE(ret);

    writer.pid_ = -1;
    ret = writer.ReadProcessStat(info);
    EXPECT_FALSE(ret);
    ret = writer.ReadProcessStatus(info);
    EXPECT_FALSE(ret);

    ret = writer.ReadProcessComm(info);
    EXPECT_FALSE(ret);

    ret = writer.ReadProcessCmdline(info);
    EXPECT_FALSE(ret);

    GTEST_LOG_(INFO) << "PrpsinfoWriter001: end.";
}

/**
 * @tc.name: AuxvWriter001
 * @tc.desc: test AuxvWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, AuxvWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AuxvWriter001: start.";
    std::vector<char> buf(1024);
    CoredumpBufferWriter bw(buf.data(), buf.size());

    AuxvWriter writer(gettid(), bw);
    bool ret = writer.Write();
    EXPECT_TRUE(ret);

    Elf64_Nhdr note;
    ret = writer.ReadProcessAuxv(&note);
    EXPECT_TRUE(ret);

    bw.Reset();
    writer.pid_ = -1;
    ret = writer.ReadProcessAuxv(&note);
    EXPECT_FALSE(ret);

    bw.capacity_ = sizeof(Elf64_Nhdr) + 1;
    ret = writer.Write();
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "AuxvWriter001: end.";
}
}
