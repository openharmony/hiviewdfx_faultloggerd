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
using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

class MinidumpContextTest : public testing::Test {};
class MinidumpExceptionTest : public testing::Test {};

HWTEST_F(MinidumpContextTest, ContextTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Valid());
    EXPECT_EQ(ctx.GetContextCPU(), 0u);
}

HWTEST_F(MinidumpContextTest, ContextTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_FALSE(ctx.Valid());
}

HWTEST_F(MinidumpContextTest, ContextTest003, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    rawCtx.iregs[31] = 0x8000;
    rawCtx.iregs[32] = 0x1000;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_TRUE(ctx.Valid());
    EXPECT_EQ(ctx.GetContextCPU(), MD_CONTEXT_ARM64);
    EXPECT_NE(ctx.GetContextARM64(), nullptr);

    uint64_t pc = 0;
    EXPECT_TRUE(ctx.GetProgramCounter(pc));
    EXPECT_EQ(pc, 0x1000u);

    uint64_t sp = 0;
    EXPECT_TRUE(ctx.GetStackPointer(sp));
    EXPECT_EQ(sp, 0x8000u);
}

HWTEST_F(MinidumpContextTest, ContextTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    uint64_t pc = 0;
    EXPECT_FALSE(ctx.GetProgramCounter(pc));
    uint64_t sp = 0;
    EXPECT_FALSE(ctx.GetStackPointer(sp));
}

HWTEST_F(MinidumpContextTest, ContextTest005, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = 0;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_EQ(ctx.GetContextCPU(), 0u);
    EXPECT_EQ(ctx.GetContextARM64(), nullptr);
    uint64_t pc = 0;
    EXPECT_FALSE(ctx.GetProgramCounter(pc));
}

HWTEST_F(MinidumpContextTest, ContextTest006, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_TRUE(ctx.Valid());
    ctx.Print();
}

HWTEST_F(MinidumpContextTest, ContextTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Valid());
    ctx.Print();
}

HWTEST_F(MinidumpContextTest, ContextTest008, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = 0x00010000;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    uint64_t sp = 0;
    EXPECT_FALSE(ctx.GetStackPointer(sp));
}

HWTEST_F(MinidumpExceptionTest, ExceptionTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    EXPECT_FALSE(exc.Valid());
    EXPECT_EQ(exc.Exception(), nullptr);
    EXPECT_EQ(exc.ExceptionRecord(), nullptr);
}

HWTEST_F(MinidumpExceptionTest, ExceptionTest002, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 123;
    rawExc.exceptionRecord.exceptionCode = 11;
    rawExc.exceptionRecord.exceptionFlags = 0;
    rawExc.exceptionRecord.exceptionAddress = 0x4000;
    rawExc.threadContext.dataSize = 0;
    rawExc.threadContext.rva = 0;
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));
    EXPECT_TRUE(exc.Valid());
    EXPECT_NE(exc.Exception(), nullptr);
    EXPECT_EQ(exc.Exception()->threadId, 123u);
    EXPECT_NE(exc.ExceptionRecord(), nullptr);
    EXPECT_EQ(exc.ExceptionRecord()->exceptionCode, 11u);

    uint32_t tid = 0;
    EXPECT_TRUE(exc.GetThreadID(tid));
    EXPECT_EQ(tid, 123u);
}

HWTEST_F(MinidumpExceptionTest, ExceptionTest003, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    uint32_t tid = 0;
    EXPECT_FALSE(exc.GetThreadID(tid));
    EXPECT_EQ(exc.GetContext(), nullptr);
}

HWTEST_F(MinidumpExceptionTest, ExceptionTest004, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 456;
    rawExc.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawExc.threadContext.rva = sizeof(MDExceptionStream);
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    data += std::string(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));
    auto ctx = exc.GetContext();
    EXPECT_NE(ctx, nullptr);
    EXPECT_TRUE(ctx->Valid());
}

HWTEST_F(MinidumpExceptionTest, ExceptionTest005, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 100;
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));
    EXPECT_TRUE(exc.Valid());
    exc.Print();
}

HWTEST_F(MinidumpExceptionTest, ExceptionTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    EXPECT_FALSE(exc.Valid());
    exc.Print();
}

} // namespace HiviewDFX
} // namespace OHOS
