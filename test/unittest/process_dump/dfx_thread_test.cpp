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

#include <gtest/gtest.h>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_process.h"
#include "dfx_ptrace.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxThreadTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: DfxThreadCreate
 * @tc.desc: test DfxThread creation with pid and tid
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_Create_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_Create_001: start.";
    pid_t pid = getpid();
    pid_t tid = gettid();
    auto thread = DfxThread::Create(pid, tid, tid, false);
    ASSERT_TRUE(thread != nullptr);
    ASSERT_EQ(thread->GetThreadInfo().pid, pid);
    ASSERT_EQ(thread->GetThreadInfo().tid, tid);
    ASSERT_EQ(thread->GetThreadInfo().nsTid, tid);
    GTEST_LOG_(INFO) << "DfxThread_Create_001: end.";
}

/**
 * @tc.name: DfxThreadAddFrameGetFrames
 * @tc.desc: test DfxThread adding and getting frames
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_AddFrameGetFrames_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_AddFrameGetFrames_002: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    DfxFrame frame;
    frame.pc = 0x1000;
    frame.funcName = "testFunc";
    frame.mapName = "test.so";
    thread->AddFrame(frame);
    const auto& frames = thread->GetFrames();
    ASSERT_EQ(frames.size(), 1);
    ASSERT_EQ(frames[0].pc, 0x1000);
    ASSERT_EQ(frames[0].funcName, "testFunc");
    GTEST_LOG_(INFO) << "DfxThread_AddFrameGetFrames_002: end.";
}

/**
 * @tc.name: DfxThreadSetFrames
 * @tc.desc: test DfxThread setting multiple frames
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_SetFrames_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_SetFrames_003: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    vector<DfxFrame> frames;
    DfxFrame f1;
    f1.pc = 0x100;
    DfxFrame f2;
    f2.pc = 0x200;
    frames.push_back(f1);
    frames.push_back(f2);
    thread->SetFrames(frames);
    ASSERT_EQ(thread->GetFrames().size(), 2);
    ASSERT_EQ(thread->GetFrames()[0].pc, 0x100);
    ASSERT_EQ(thread->GetFrames()[1].pc, 0x200);
    GTEST_LOG_(INFO) << "DfxThread_SetFrames_003: end.";
}

/**
 * @tc.name: DfxThreadToStringWithTid
 * @tc.desc: test DfxThread toString output with tid info
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_ToString_WithTid_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_ToString_004: start.";
    pid_t pid = getpid();
    pid_t tid = gettid();
    auto thread = DfxThread::Create(pid, tid, tid, false);
    thread->SetThreadName("testThread");
    string result = thread->ToString(true);
    ASSERT_TRUE(result.find("Tid:") != string::npos);
    ASSERT_TRUE(result.find("Name:") != string::npos);
    GTEST_LOG_(INFO) << "DfxThread_ToString_004: end.";
}

/**
 * @tc.name: DfxThreadToStringNoFrames
 * @tc.desc: test DfxThread toString output without frames
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_ToString_NoFrames_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_ToString_NoFrames_005: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    thread->SetFrames({});
    string result = thread->ToString(false);
    ASSERT_TRUE(result.find("No frame info") != string::npos);
    GTEST_LOG_(INFO) << "DfxThread_ToString_NoFrames_005: end.";
}

/**
 * @tc.name: DfxThreadToStringWithFrames
 * @tc.desc: test DfxThread toString output with frames
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_ToString_WithFrames_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_ToString_WithFrames_006: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    thread->SetThreadName("myThread");
    Unwinder unwinder;
    unwinder.UnwindLocal();
    thread->SetFrames(unwinder.GetFrames());
    string result = thread->ToString(false);
    ASSERT_TRUE(result.find("Thread name:") != string::npos);
    GTEST_LOG_(INFO) << "DfxThread_ToString_WithFrames_006: end.";
}

/**
 * @tc.name: DfxThreadFillSymbol
 * @tc.desc: test DfxThread filling symbol from frame table
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_FillSymbol_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_FillSymbol_007: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    vector<DfxFrame> frames;
    DfxFrame f;
    f.pc = 0x1000;
    f.isJsFrame = false;
    f.funcName = "";
    f.funcOffset = 0;
    frames.push_back(f);
    thread->SetFrames(frames);

    map<uint64_t, DfxFrame> frameTable;
    DfxFrame tableFrame;
    tableFrame.pc = 0x1000;
    tableFrame.funcName = "resolvedFunc";
    tableFrame.funcOffset = 10;
    tableFrame.parseSymbolState.SetParseSymbolState(true);
    frameTable[0x1000] = tableFrame;

    thread->FillSymbol(frameTable);
    ASSERT_EQ(thread->GetFrames()[0].funcName, "resolvedFunc");
    ASSERT_EQ(thread->GetFrames()[0].funcOffset, 10);
    GTEST_LOG_(INFO) << "DfxThread_FillSymbol_007: end.";
}

/**
 * @tc.name: DfxThreadFillSymbolNoParse
 * @tc.desc: test DfxThread FillSymbol when parse symbol is disabled
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_FillSymbol_NoParse_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_FillSymbol_NoParse_008: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    vector<DfxFrame> frames;
    DfxFrame f;
    f.pc = 0x1000;
    f.funcName = "";
    frames.push_back(f);
    thread->SetFrames(frames);
    thread->SetParseSymbolNecessity(false);

    map<uint64_t, DfxFrame> frameTable;
    thread->FillSymbol(frameTable);
    ASSERT_EQ(thread->GetFrames()[0].funcName, "");
    GTEST_LOG_(INFO) << "DfxThread_FillSymbol_NoParse_008: end.";
}

/**
 * @tc.name: DfxThreadSetParseSymbolNecessity
 * @tc.desc: test DfxThread setting parse symbol necessity flag
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_SetParseSymbolNecessity_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_SetParseSymbolNecessity_009: start.";
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    thread->SetParseSymbolNecessity(true);
    thread->SetParseSymbolNecessity(false);
    vector<DfxFrame> frames;
    DfxFrame f;
    f.pc = 0x1000;
    f.funcName = "";
    frames.push_back(f);
    thread->SetFrames(frames);
    thread->FillSymbol({});
    ASSERT_EQ(thread->GetFrames()[0].funcName, "");
    GTEST_LOG_(INFO) << "DfxThread_SetParseSymbolNecessity_009: end.";
}

/**
 * @tc.name: DfxThreadSetGetThreadRegs
 * @tc.desc: test DfxThread setting and getting thread registers
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_SetGetThreadRegs_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThread_SetGetThreadRegs_010: start.";
    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        exit(0);
    }
    auto thread = DfxThread::Create(pid, pid, pid, false);
    ASSERT_TRUE(thread->GetThreadRegs() == nullptr);
    ASSERT_TRUE(DfxPtrace::Attach(pid));
    auto regs = DfxRegs::CreateRemoteRegs(pid);
    thread->SetThreadRegs(regs);
    ASSERT_TRUE(thread->GetThreadRegs() != nullptr);
    GTEST_LOG_(INFO) << "DfxThread_SetGetThreadRegs_010: end.";
}

/**
 * @tc.name: DfxThreadSetGetThreadStackBuffer
 * @tc.desc: test DfxThread setting and getting thread stack buffer
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_SetGetStackBuffer_011, TestSize.Level2)
{
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    ASSERT_TRUE(thread->GetThreadStackBuffer() == nullptr);
    auto stackBuf = std::make_shared<std::vector<uint8_t>>(100, 0xAB);
    thread->SetThreadStackBuffer(stackBuf);
    auto retrievedBuf = thread->GetThreadStackBuffer();
    ASSERT_TRUE(retrievedBuf != nullptr);
    ASSERT_EQ(retrievedBuf->size(), 100);
    ASSERT_EQ(retrievedBuf->at(0), 0xAB);
    thread->SetThreadStackBuffer(nullptr);
    ASSERT_TRUE(thread->GetThreadStackBuffer() == nullptr);
}

/**
 * @tc.name: DfxThreadSetGetStartOfStackMemory
 * @tc.desc: test DfxThread setting and getting start of stack memory
 * @tc.type: FUNC
 */
HWTEST_F(DfxThreadTest, DfxThread_SetGetStartOfStackMemory_012, TestSize.Level2)
{
    pid_t pid = getpid();
    auto thread = DfxThread::Create(pid, gettid(), gettid(), false);
    ASSERT_EQ(thread->GetStartOfStackMemory(), 0);
    thread->SetStartOfStackMemory(0x10000);
    ASSERT_EQ(thread->GetStartOfStackMemory(), 0x10000);
    thread->SetStartOfStackMemory(0x20000);
    ASSERT_EQ(thread->GetStartOfStackMemory(), 0x20000);
    thread->SetStartOfStackMemory(0);
    ASSERT_EQ(thread->GetStartOfStackMemory(), 0);
}
}