/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>

#include "dfx_elf.h"
#include "dfx_maps.h"
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "dfx_regs_get.h"
#include "dfx_symbols.h"
#include "dfx_ptrace.h"
#include "dfx_test_util.h"
#include "dwarf_define.h"
#include "stack_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxMemoryTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: DfxMemoryTest001
 * @tc.desc: test DfxMemory class ReadReg
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest001: start.";
    uintptr_t regs[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa};
    UnwindContext ctx;
    ctx.regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regs, sizeof(regs) / sizeof(regs[0]));
    auto acc = std::make_shared<DfxAccessorsLocal>();
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t value;
    bool ret = memory->ReadReg(0, &value);
    EXPECT_EQ(true, ret) << "DfxMemoryTest001: ret" << ret;
    EXPECT_EQ(static_cast<uintptr_t>(0x1), value) << "DfxMemoryTest001: value" << value;
    GTEST_LOG_(INFO) << "DfxMemoryTest001: end.";
}

/**
 * @tc.name: DfxMemoryTest002
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest002: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};

    uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
    uintptr_t value;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    auto memory = std::make_shared<DfxMemory>(acc);
    bool ret = memory->ReadUptr(addr, &value, false);
    EXPECT_EQ(true, ret) << "DfxMemoryTest002: ret:" << ret;


    uint64_t tmp;
    memory->Read(addr, &tmp, sizeof(uint8_t), false);
    ASSERT_EQ(tmp, 0x01);

    memory->Read(addr, &tmp, sizeof(uint16_t), false);
    ASSERT_EQ(tmp, 0x0201);

    memory->Read(addr, &tmp, sizeof(uint32_t), false);
    ASSERT_EQ(tmp, 0x04030201);

    memory->Read(addr, &tmp, sizeof(uint64_t), false);
    ASSERT_EQ(tmp, 0x0807060504030201);

    GTEST_LOG_(INFO) << "DfxMemoryTest002: end.";
}

/**
 * @tc.name: DfxMemoryTest003
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest003: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    UnwindContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_TRUE(GetSelfStackRange(ctx.stackBottom, ctx.stackTop));
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
    uintptr_t value;
    ASSERT_TRUE(memory->ReadUptr(addr, &value, false));
#if defined(__arm__)
    ASSERT_EQ(value, 0x04030201);
#elif defined(__aarch64__)
    ASSERT_EQ(value, 0x0807060504030201);
#endif

    uint64_t tmp;
    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint8_t), false));
    ASSERT_EQ(tmp, 0x01);

    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint16_t), false));
    ASSERT_EQ(tmp, 0x0201);

    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint32_t), false));
    ASSERT_EQ(tmp, 0x04030201);

    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint64_t), false));
    ASSERT_EQ(tmp, 0x0807060504030201);

    GTEST_LOG_(INFO) << "DfxMemoryTest003: end.";
}

/**
 * @tc.name: DfxMemoryTest004
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest004: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    UnwindContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_TRUE(GetSelfStackRange(ctx.stackBottom, ctx.stackTop));
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
    uint8_t tmp8;
    ASSERT_TRUE(memory->ReadU8(addr, &tmp8, false));
    ASSERT_EQ(tmp8, 0x01);
    uint16_t tmp16;
    ASSERT_TRUE(memory->ReadU16(addr, &tmp16, false));
    ASSERT_EQ(tmp16, 0x0201);
    uint32_t tmp32;
    ASSERT_TRUE(memory->ReadU32(addr, &tmp32, false));
    ASSERT_EQ(tmp32, 0x04030201);
    uint64_t tmp64;
    ASSERT_TRUE(memory->ReadU64(addr, &tmp64, false));
    ASSERT_EQ(tmp64, 0x0807060504030201);
    GTEST_LOG_(INFO) << "DfxMemoryTest004: end.";
}

/**
 * @tc.name: DfxMemoryTest005
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest005: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    UnwindContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_TRUE(GetSelfStackRange(ctx.stackBottom, ctx.stackTop));
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
    uintptr_t valuePrel32;
    ASSERT_TRUE(memory->ReadPrel31(addr, &valuePrel32));
    ASSERT_EQ(valuePrel32, 0x04030201 + addr);
    char testStr[] = "Test ReadString Func";
    std::string resultStr;
    uintptr_t addrStr = reinterpret_cast<uintptr_t>(&testStr[0]);
    ASSERT_TRUE(memory->ReadString(addrStr, &resultStr, sizeof(testStr)/sizeof(char), false));
    ASSERT_EQ(testStr, resultStr);
    ASSERT_EQ(memory->ReadUleb128(addr), 1U);
    ASSERT_EQ(memory->ReadSleb128(addr), 2);
    GTEST_LOG_(INFO) << "DfxMemoryTest005: end.";
}

/**
 * @tc.name: DfxMemoryTest006
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest006: start.";
    UnwindContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_TRUE(GetSelfStackRange(ctx.stackBottom, ctx.stackTop));
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_absptr), sizeof(uintptr_t));
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata1), 1);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata2), 2);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata4), 4);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata8), 8);
    GTEST_LOG_(INFO) << "DfxMemoryTest006: end.";
}

/**
 * @tc.name: DfxMemoryTest007
 * @tc.desc: test DfxMemory class ReadReg in remote case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest007: start.";
    uintptr_t regs[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa};
    UnwindContext ctx;
    ctx.regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regs, sizeof(regs) / sizeof(regs[0]));
    auto acc = std::make_shared<DfxAccessorsRemote>();
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t value;
    bool ret = memory->ReadReg(0, &value);
    EXPECT_EQ(true, ret) << "DfxMemoryTest007: ret" << ret;
    EXPECT_EQ(static_cast<uintptr_t>(0x1), value) << "DfxMemoryTest007: value" << value;
    GTEST_LOG_(INFO) << "DfxMemoryTest007: end.";
}
/**
 * @tc.name: DfxMemoryTest008
 * @tc.desc: test DfxMemory class Read in remote case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest008: start.";
    static pid_t pid = getpid();
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        DfxPtrace::Attach(pid);
        uintptr_t value;
        UnwindContext ctx;
        ctx.pid = pid;
        auto acc = std::make_shared<DfxAccessorsRemote>();
        auto memory = std::make_shared<DfxMemory>(acc);
        memory->SetCtx(&ctx);
        uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
        bool ret = memory->ReadUptr(addr, &value, false);
        EXPECT_EQ(true, ret) << "DfxMemoryTest008: ret:" << ret;
        uint64_t tmp;
        memory->Read(addr, &tmp, sizeof(uint8_t), false);
        EXPECT_EQ(tmp, 0x01);

        memory->Read(addr, &tmp, sizeof(uint16_t), false);
        EXPECT_EQ(tmp, 0x0201);

        memory->Read(addr, &tmp, sizeof(uint32_t), false);
        EXPECT_EQ(tmp, 0x04030201);

        memory->Read(addr, &tmp, sizeof(uint64_t), false);
        EXPECT_EQ(tmp, 0x0807060504030201);
        DfxPtrace::Detach(pid);
        _exit(0);
    }
    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "DfxMemoryTest008: end.";
}

/**
 * @tc.name: DfxMemoryTest009
 * @tc.desc: test DfxMemory class Read in remote case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest009: start.";
    static pid_t pid = getpid();
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        DfxPtrace::Attach(pid);
        UnwindContext ctx;
        ctx.pid = pid;
        auto acc = std::make_shared<DfxAccessorsRemote>();
        auto memory = std::make_shared<DfxMemory>(acc);
        memory->SetCtx(&ctx);
        uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
        uintptr_t value;
        ASSERT_TRUE(memory->ReadUptr(addr, &value, false));
#if defined(__arm__)
        ASSERT_EQ(value, 0x04030201);
#elif defined(__aarch64__)
        ASSERT_EQ(value, 0x0807060504030201);
#endif

        uint64_t tmp;
        ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint8_t), false));
        ASSERT_EQ(tmp, 0x01);

        ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint16_t), false));
        ASSERT_EQ(tmp, 0x0201);

        ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint32_t), false));
        ASSERT_EQ(tmp, 0x04030201);

        ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint64_t), false));
        ASSERT_EQ(tmp, 0x0807060504030201);
        DfxPtrace::Detach(pid);
        _exit(0);
    }
    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "DfxMemoryTest009: end.";
}

/**
 * @tc.name: DfxMemoryTest010
 * @tc.desc: test DfxMemory class Read in remote case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest010: start.";
    static pid_t pid = getpid();
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        DfxPtrace::Attach(pid);

        UnwindContext ctx;
        ctx.pid = pid;
        auto acc = std::make_shared<DfxAccessorsRemote>();
        auto memory = std::make_shared<DfxMemory>(acc);
        memory->SetCtx(&ctx);
        uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
        uint8_t tmp8;
        ASSERT_TRUE(memory->ReadU8(addr, &tmp8, false));
        ASSERT_EQ(tmp8, 0x01);
        uint16_t tmp16;
        ASSERT_TRUE(memory->ReadU16(addr, &tmp16, false));
        ASSERT_EQ(tmp16, 0x0201);
        uint32_t tmp32;
        ASSERT_TRUE(memory->ReadU32(addr, &tmp32, false));
        ASSERT_EQ(tmp32, 0x04030201);
        uint64_t tmp64;
        ASSERT_TRUE(memory->ReadU64(addr, &tmp64, false));
        ASSERT_EQ(tmp64, 0x0807060504030201);
        DfxPtrace::Detach(pid);
        _exit(0);
    }
    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "DfxMemoryTest010: end.";
}

/**
 * @tc.name: DfxMemoryTest011
 * @tc.desc: test DfxMemory class Read in remote case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest011: start.";
    static pid_t pid = getpid();
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    char testStr[] = "Test ReadString Func";
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        DfxPtrace::Attach(getppid());
        UnwindContext ctx;
        ctx.pid = getppid();
        auto acc = std::make_shared<DfxAccessorsRemote>();
        auto memory = std::make_shared<DfxMemory>(acc);
        memory->SetCtx(&ctx);
        uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
        std::string resultStr;
        uintptr_t addrStr = reinterpret_cast<uintptr_t>(&testStr[0]);
        ASSERT_TRUE(memory->ReadString(addrStr, &resultStr, sizeof(testStr)/sizeof(char), false));
        ASSERT_EQ(testStr, resultStr);
        ASSERT_TRUE(memory->ReadString(addrStr, &resultStr, sizeof(testStr)/sizeof(char), true));
        ASSERT_EQ(testStr, resultStr);
        ASSERT_EQ(memory->ReadUleb128(addr), 1U);
        ASSERT_EQ(memory->ReadSleb128(addr), 2);
        DfxPtrace::Detach(pid);
        _exit(0);
    }
    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "DfxMemoryTest011: end.";
}

/**
 * @tc.name: DfxMemoryTest012
 * @tc.desc: test DfxMemory class Read in remote case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest012: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        DfxPtrace::Attach(pid);
        UnwindContext ctx;
        ctx.pid = pid;
        auto acc = std::make_shared<DfxAccessorsRemote>();
        auto memory = std::make_shared<DfxMemory>(acc);
        memory->SetCtx(&ctx);
        ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_absptr), sizeof(uintptr_t));
        ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata1), 1);
        ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata2), 2);
        ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata4), 4);
        ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata8), 8);
        DfxPtrace::Detach(pid);
        _exit(0);
    }
    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "DfxMemoryTest012: end.";
}

/**
 * @tc.name: DfxMemoryTest013
 * @tc.desc: test DfxMemory class Read in error case
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest013: start.";
    auto acc = std::make_shared<DfxAccessorsLocal>();
    auto memory = std::make_shared<DfxMemory>(acc);
    uintptr_t val;
    EXPECT_FALSE(memory->ReadReg(0, &val));
    uintptr_t regs[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa};
    UnwindContext ctx;
    ctx.regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regs, sizeof(regs) / sizeof(regs[0]));
    memory->SetCtx(&ctx);
    EXPECT_FALSE(memory->ReadReg(-1, &val));

    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
    EXPECT_FALSE(memory->ReadMem(addr, nullptr));
    EXPECT_FALSE(memory->ReadUptr(addr, nullptr, false));
    EXPECT_FALSE(memory->Read(addr, nullptr, sizeof(uint8_t), false));
    EXPECT_FALSE(memory->ReadU8(addr, nullptr, false));
    EXPECT_FALSE(memory->ReadU16(addr, nullptr, false));
    EXPECT_FALSE(memory->ReadU32(addr, nullptr, false));
    EXPECT_FALSE(memory->ReadU64(addr, nullptr, false));
    GTEST_LOG_(INFO) << "DfxMemoryTest013: end.";
}

/**
 * @tc.name: DfxMemoryTest014
 * @tc.desc: test DfxMemory class ReadProcMemByPid
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest014: start.";
    pid_t pid = GetProcessPid(FOUNDATION_NAME);
    auto maps = DfxMaps::Create(pid);
    std::vector<std::shared_ptr<DfxMap>> shmmMaps;
    bool isSuccess = maps->FindMapsByName("shmm", shmmMaps);
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(INFO) << "DfxMemoryTest014: Failed to find shmm";
        return;
    }
    std::shared_ptr<DfxMap> shmmMap = nullptr;
    for (auto map : shmmMaps) {
        if (map->IsMapExec()) {
            shmmMap = map;
            break;
        }
    }
    ASSERT_TRUE(shmmMap != nullptr);
    auto shmmData = std::make_shared<std::vector<uint8_t>>(shmmMap->end - shmmMap->begin);
    DfxMemory::ReadProcMemByPid(pid, shmmMap->begin, shmmData->data(), shmmMap->end - shmmMap->begin);
    auto shmmElf = std::make_shared<DfxElf>(shmmData->data(), shmmMap->end - shmmMap->begin);
    ASSERT_TRUE(shmmElf->IsValid());
    std::vector<DfxSymbol> shmmSyms;
    DfxSymbols::ParseSymbols(shmmSyms, shmmElf, "");
    GTEST_LOG_(INFO) << "shmm symbols size" << shmmSyms.size();
    ASSERT_GT(shmmSyms.size(), 0);
    for (auto& sym : shmmSyms) {
        GTEST_LOG_(INFO) << sym.ToDebugString();
    }
    GTEST_LOG_(INFO) << "DfxMemoryTest014 : end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS
