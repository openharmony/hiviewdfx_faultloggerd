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

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "dfx_coredump_service.h"
#include "dfx_coredump_writer.h"

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
    std::vector<char> buffer(bufferSize, 0);
    char* mappedMemory = buffer.data();
    char* currentPointer = mappedMemory;
    Elf64_Half phNum = 0;
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

    std::vector<DumpMemoryRegions> maps = { dummyRegion1, dummyRegion2 };
    ProgramSegmentHeaderWriter writer(mappedMemory, currentPointer, phNum, maps);

    currentPointer = writer.Write();

    ASSERT_TRUE(static_cast<const void*>(currentPointer) != static_cast<const void*>(mappedMemory));
    ASSERT_TRUE(phNum > 0);

    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest001: end.";
}

/**
 * @tc.name: DfxCoreDumpWriterTest002
 * @tc.desc: test LoadSegmentWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, DfxCoreDumpWriterTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest002: start.";

    constexpr size_t kRegionSize = 64;
    auto* sourceBuffer = static_cast<char*>(malloc(kRegionSize));
    (void)memset_s(sourceBuffer, kRegionSize, 0xA5, kRegionSize);

    uintptr_t fakeVaddr = reinterpret_cast<uintptr_t>(sourceBuffer);

    constexpr size_t bufferSize = 8192;
    std::vector<char> buffer(bufferSize, 0);
    char* mappedMemory = buffer.data();
    char* currentPointer = mappedMemory + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);

    Elf64_Phdr* ptLoad = reinterpret_cast<Elf64_Phdr*>(mappedMemory + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr));

    ptLoad->p_vaddr = fakeVaddr;
    ptLoad->p_memsz = kRegionSize;

    pid_t testPid = getpid();
    Elf64_Half phNum = 2;

    LoadSegmentWriter writer(mappedMemory, currentPointer, testPid, phNum);

    char* result = writer.Write();

    ASSERT_TRUE(static_cast<const void*>(currentPointer) != static_cast<const void*>(result));

    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest002: end.";
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
    char* mappedMemory = buffer.data();
    char* currentPointer = mappedMemory;

    SectionHeaderTableWriter writer(mappedMemory, currentPointer);
    char* result = writer.Write();

    ASSERT_TRUE(static_cast<const void*>(currentPointer) != static_cast<const void*>(result));
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest003: end.";
}

/**
 * @tc.name: DfxCoreDumpWriterTest004
 * @tc.desc: test NoteSegmentWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpWriterTest, DfxCoreDumpWriterTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest004: start.";
    constexpr size_t bufferSize = 4096;
    std::vector<char> buffer(bufferSize, 0);
    char* mappedMemory = buffer.data();
    char* currentPointer = mappedMemory;

    CoreDumpThread coreDumpThread;
    coreDumpThread.targetPid = getpid();
    coreDumpThread.targetTid = gettid();
    DumpMemoryRegions dummyRegion {
        .startHex = 0x1000,
        .endHex = 0x2000,
        .offsetHex = 5,
        .memorySizeHex = 0x1000
    };
    std::vector<DumpMemoryRegions> maps = { dummyRegion };
    std::shared_ptr<DfxRegs> regs = DfxRegs::Create();

    NoteSegmentWriter writer(mappedMemory, currentPointer, coreDumpThread, maps, regs);
    char* result = writer.Write();

    ASSERT_TRUE(static_cast<const void*>(currentPointer) != static_cast<const void*>(result));
    GTEST_LOG_(INFO) << "DfxCoreDumpWriterTest004: end.";
}
}