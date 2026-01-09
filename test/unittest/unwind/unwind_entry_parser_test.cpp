/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <dlfcn.h>
#include <cstdint>

#include "dfx_maps.h"
#include "dfx_memory.h"
#include "dfx_ptrace.h"
#include "dfx_regs.h"
#include "dfx_regs_get.h"
#include "dfx_test_util.h"
#include "unwind_context.h"
#include "unwind_define.h"
#include "unwind_entry_parser_factory.h"


using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class UnwindEntryParserTest : public testing::Test {};

/**
 * @tc.name: UnwindEntryParser001
 * @tc.desc: test UnwindEntryParser in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwindEntryParserTest, UnwindEntryParseTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindEntryParser001: start.";
    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    UnwindContext context;
    auto memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    memory->SetCtx(&context);
    UnwindTableInfo uti;
    ASSERT_TRUE(memory->FindUnwindTable(regs->GetPc(), uti) == UNW_ERROR_NONE);
    auto unwindEntryParser = UnwindEntryParserFactory::CreateUnwindEntryParser(memory);
    std::shared_ptr<RegLocState> rs = std::make_shared<RegLocState>();
    ASSERT_TRUE(unwindEntryParser->Step(regs->GetPc(), uti, rs));
    GTEST_LOG_(INFO) << "UnwindEntryParser001: end.";
}

/**
 * @tc.name: UnwindEntryParser002
 * @tc.desc: test UnwindEntryParser in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwindEntryParserTest, UnwindEntryParseTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindEntryParser002: start.";
    pid_t childPid = fork();
    if (childPid != 0) {
        ASSERT_TRUE(DfxPtrace::Attach(childPid));
        auto regs = DfxRegs::CreateRemoteRegs(childPid);
        UnwindContext context;
        context.pid = childPid;
        context.regs = regs;
        context.maps = DfxMaps::Create(childPid);
        auto memory = std::make_shared<DfxMemory>(UNWIND_TYPE_REMOTE);
        memory->SetCtx(&context);
        std::shared_ptr<DfxMap> map = nullptr;
        uintptr_t pc = regs->GetPc();
        ASSERT_TRUE(memory->GetMapByPc(pc, map) == UNW_ERROR_NONE);
        UnwindTableInfo uti;
        ASSERT_TRUE(memory->FindUnwindTable(pc, uti) == UNW_ERROR_NONE);
        auto unwindEntryParser = UnwindEntryParserFactory::CreateUnwindEntryParser(memory);
        std::shared_ptr<RegLocState> rs = std::make_shared<RegLocState>();
        ASSERT_TRUE(unwindEntryParser->Step(pc, uti, rs));
        DfxPtrace::Detach(childPid);
    } else {
        const int sleepTime = 2;
        sleep(sleepTime);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "UnwindEntryParser002: end.";
}

/**
 * @tc.name: UnwindEntryParser003
 * @tc.desc: test UnwindEntryParser in abnomal case
 * @tc.type: FUNC
 */
HWTEST_F(UnwindEntryParserTest, UnwindEntryParseTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindEntryParser003: start.";
    std::shared_ptr <DfxMemory> memory = nullptr;
    int pc = 0;
    auto unwindEntryParser = UnwindEntryParserFactory::CreateUnwindEntryParser(memory);
    std::shared_ptr<RegLocState> rs = std::make_shared<RegLocState>();
    UnwindTableInfo uti;
    ASSERT_FALSE(unwindEntryParser->Step(pc, uti, rs));
    GTEST_LOG_(INFO) << "UnwindEntryParser003: end.";
}

/**
 * @tc.name: UnwindEntryParser004
 * @tc.desc: test UnwindEntryParser in abnomal pc case
 * @tc.type: FUNC
 */
HWTEST_F(UnwindEntryParserTest, UnwindEntryParseTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindEntryParser004: start.";
    pid_t childPid = fork();
    if (childPid != 0) {
        ASSERT_TRUE(DfxPtrace::Attach(childPid));
        auto regs = DfxRegs::CreateRemoteRegs(childPid);
        UnwindContext context;
        context.pid = childPid;
        context.regs = regs;
        context.maps = DfxMaps::Create(childPid);
        auto memory = std::make_shared<DfxMemory>(UNWIND_TYPE_REMOTE);
        memory->SetCtx(&context);
        uintptr_t pc = regs->GetPc();
        std::shared_ptr<DfxMap> map = nullptr;
        ASSERT_TRUE(memory->GetMapByPc(pc, map) == UNW_ERROR_NONE);
        UnwindTableInfo uti;
        ASSERT_TRUE(memory->FindUnwindTable(pc, uti) == UNW_ERROR_NONE);
        auto unwindEntryParser = UnwindEntryParserFactory::CreateUnwindEntryParser(memory);
        std::shared_ptr<RegLocState> rs = std::make_shared<RegLocState>();
        ASSERT_FALSE(unwindEntryParser->Step(0, uti, rs));
        DfxPtrace::Detach(childPid);
    } else {
        const int sleepTime = 2;
        sleep(sleepTime);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "UnwindEntryParser004: end.";
}

/**
 * @tc.name: UnwindEntryParser005
 * @tc.desc: test UnwindEntryParser in abnomal UnwindTableInfo case
 * @tc.type: FUNC
 */
HWTEST_F(UnwindEntryParserTest, UnwindEntryParseTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindEntryParser005: start.";
    pid_t childPid = fork();
    if (childPid != 0) {
        ASSERT_TRUE(DfxPtrace::Attach(childPid));
        auto regs = DfxRegs::CreateRemoteRegs(childPid);
        UnwindContext context;
        context.pid = childPid;
        context.regs = regs;
        context.maps = DfxMaps::Create(childPid);
        auto memory = std::make_shared<DfxMemory>(UNWIND_TYPE_REMOTE);
        memory->SetCtx(&context);
        auto unwindEntryParser = UnwindEntryParserFactory::CreateUnwindEntryParser(memory);
        UnwindTableInfo uti;
        std::shared_ptr<RegLocState> rs = std::make_shared<RegLocState>();
        ASSERT_FALSE(unwindEntryParser->Step(regs->GetPc(), uti, rs));
        DfxPtrace::Detach(childPid);
    } else {
        const int sleepTime = 2;
        sleep(sleepTime);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "UnwindEntryParser005: end.";
}
}
}