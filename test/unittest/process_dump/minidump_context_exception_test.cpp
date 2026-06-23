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

constexpr uint64_t TEST_STACK_POINTER_VALUE = 0x8000;
constexpr uint64_t TEST_PROGRAM_COUNTER_VALUE = 0x1000;
constexpr uint64_t TEST_EXCEPTION_ADDRESS = 0x4000;
constexpr uint32_t MD_CONTEXT_INTEGER_FLAG = 0x00010000;

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpContextTest : public testing::Test {};
class MinidumpExceptionTest : public testing::Test {};

/**
 * @tc.name: ContextTest001
 * @tc.desc: test MinidumpContext validity and CPU type with empty reader
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Valid());
    EXPECT_EQ(ctx.GetContextCPU(), 0u);
}

/**
 * @tc.name: ContextTest002
 * @tc.desc: test MinidumpContext Read with empty reader returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_FALSE(ctx.Valid());
}

/**
 * @tc.name: ContextTest003
 * @tc.desc: test MinidumpContext Read with valid ARM64 context data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextTest003, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    rawCtx.iregs[31] = TEST_STACK_POINTER_VALUE;
    rawCtx.iregs[32] = TEST_PROGRAM_COUNTER_VALUE;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_TRUE(ctx.Valid());
    EXPECT_EQ(ctx.GetContextCPU(), MD_CONTEXT_ARM64);
    EXPECT_NE(ctx.GetContextARM64(), nullptr);

    uint64_t pc = 0;
    EXPECT_TRUE(ctx.GetProgramCounter(pc));
    EXPECT_EQ(pc, TEST_PROGRAM_COUNTER_VALUE);

    uint64_t sp = 0;
    EXPECT_TRUE(ctx.GetStackPointer(sp));
    EXPECT_EQ(sp, TEST_STACK_POINTER_VALUE);
}

/**
 * @tc.name: ContextTest004
 * @tc.desc: test MinidumpContext GetProgramCounter and GetStackPointer with invalid context
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    uint64_t pc = 0;
    EXPECT_FALSE(ctx.GetProgramCounter(pc));
    uint64_t sp = 0;
    EXPECT_FALSE(ctx.GetStackPointer(sp));
}

/**
 * @tc.name: ContextTest005
 * @tc.desc: test MinidumpContext with zero contextFlags returns invalid CPU type
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: ContextTest006
 * @tc.desc: test MinidumpContext Print with valid ARM64 context
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: ContextTest007
 * @tc.desc: test MinidumpContext Print with invalid context
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Valid());
    ctx.Print();
}

/**
 * @tc.name: ContextTest008
 * @tc.desc: test MinidumpContext GetStackPointer with non-ARM64 context flags
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextTest008, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_INTEGER_FLAG;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    uint64_t sp = 0;
    EXPECT_FALSE(ctx.GetStackPointer(sp));
}

/**
 * @tc.name: ExceptionTest001
 * @tc.desc: test MinidumpException validity with empty reader
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    EXPECT_FALSE(exc.Valid());
    EXPECT_EQ(exc.Exception(), nullptr);
    EXPECT_EQ(exc.ExceptionRecord(), nullptr);
}

/**
 * @tc.name: ExceptionTest002
 * @tc.desc: test MinidumpException Read with valid exception stream data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionTest002, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 123;
    rawExc.exceptionRecord.exceptionCode = 11;
    rawExc.exceptionRecord.exceptionFlags = 0;
    rawExc.exceptionRecord.exceptionAddress = TEST_EXCEPTION_ADDRESS;
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

/**
 * @tc.name: ExceptionTest003
 * @tc.desc: test MinidumpException GetThreadID and GetContext with invalid exception
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionTest003, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    uint32_t tid = 0;
    EXPECT_FALSE(exc.GetThreadID(tid));
    EXPECT_EQ(exc.GetContext(), nullptr);
}

/**
 * @tc.name: ExceptionTest004
 * @tc.desc: test MinidumpException GetContext with valid thread context data
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: ExceptionTest005
 * @tc.desc: test MinidumpException Print with valid exception stream
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: ExceptionTest006
 * @tc.desc: test MinidumpException Print with invalid exception
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    EXPECT_FALSE(exc.Valid());
    exc.Print();
}

/**
 * @tc.name: ExceptionSubjectNotificationTest001
 * @tc.desc: test MinidumpException subject notification after Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionSubjectNotificationTest001, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 123;
    rawExc.threadContext.dataSize = 0;
    rawExc.threadContext.rva = 0;
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);

    auto subject = std::make_shared<MinidumpSubject>();
    std::string receivedName;
    uint32_t receivedCount = 0;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            receivedName = name;
            receivedCount = count;
        });
    subject->AddObserver(obs);
    exc.SetMinidumpSubject(subject);

    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));
    EXPECT_EQ(receivedName, "Exception");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: ExceptionGetContextSeekFailureTest001
 * @tc.desc: test MinidumpException GetContext returns null on seek failure
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionGetContextSeekFailureTest001, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 456;
    rawExc.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawExc.threadContext.rva = sizeof(MDExceptionStream);
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    data += std::string(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto ss = std::make_shared<std::stringstream>(data, std::ios::binary | std::ios::in);
    auto reader = std::make_shared<MinidumpMemoryReader>(ss);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));

    ss->setstate(std::ios::badbit);
    auto ctx = exc.GetContext();
    EXPECT_EQ(ctx, nullptr);
    EXPECT_EQ(exc.GetLastError().GetError(), MinidumpError::ERROR_FILE_SEEK);
}

/**
 * @tc.name: ExceptionGetContextReadFailureTest001
 * @tc.desc: test MinidumpException GetContext returns null on read failure
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionGetContextReadFailureTest001, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 100;
    rawExc.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawExc.threadContext.rva = sizeof(MDExceptionStream);
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));

    auto ctx = exc.GetContext();
    EXPECT_EQ(ctx, nullptr);
    EXPECT_TRUE(exc.GetLastError().IsError());
}

/**
 * @tc.name: ContextGetContextFlagsTest001
 * @tc.desc: test MinidumpContext GetContextFlags with combined ARM64 flags
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextGetContextFlagsTest001, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64 | MD_CONTEXT_INTEGER_FLAG;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_TRUE(ctx.Valid());
    EXPECT_EQ(ctx.GetContextFlags(), MD_CONTEXT_ARM64 | MD_CONTEXT_INTEGER_FLAG);
}

/**
 * @tc.name: ExceptionReadWithoutSubjectTest001
 * @tc.desc: test MinidumpException Read without subject set still succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionReadWithoutSubjectTest001, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 123;
    rawExc.threadContext.dataSize = 0;
    rawExc.threadContext.rva = 0;
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));
    EXPECT_TRUE(exc.Valid());
}

/**
 * @tc.name: ExceptionReadStreamTooSmallTest001
 * @tc.desc: test MinidumpException Read with stream too small returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionReadStreamTooSmallTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpException exc(reader);
    EXPECT_FALSE(exc.Read(sizeof(MDExceptionStream) - 1));
    EXPECT_EQ(exc.GetLastError().GetError(), MinidumpError::ERROR_STREAM_READ);
}

/**
 * @tc.name: ExceptionReadReadBytesFailureTest001
 * @tc.desc: test MinidumpException Read with insufficient data returns error
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionReadReadBytesFailureTest001, TestSize.Level2)
{
    std::string data(sizeof(MDExceptionStream) / 2, '\0');
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_FALSE(exc.Read(sizeof(MDExceptionStream)));
    EXPECT_TRUE(exc.GetLastError().IsError());
}

/**
 * @tc.name: ExceptionPrintValidWithContextTest001
 * @tc.desc: test MinidumpException Print with valid exception and context data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpExceptionTest, ExceptionPrintValidWithContextTest001, TestSize.Level2)
{
    MDExceptionStream rawExc = {};
    rawExc.threadId = 100;
    rawExc.exceptionRecord.exceptionCode = 11;
    rawExc.exceptionRecord.exceptionFlags = 0;
    rawExc.exceptionRecord.exceptionAddress = TEST_EXCEPTION_ADDRESS;
    rawExc.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawExc.threadContext.rva = sizeof(MDExceptionStream);
    std::string data(reinterpret_cast<const char*>(&rawExc), sizeof(rawExc));
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    data += std::string(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpException exc(reader);
    EXPECT_TRUE(exc.Read(sizeof(MDExceptionStream)));
    EXPECT_TRUE(exc.Valid());
    exc.Print();
}

/**
 * @tc.name: ContextReadBytesFailureTest001
 * @tc.desc: test MinidumpContext Read with partial data returns error
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextReadBytesFailureTest001, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    std::string partialData(reinterpret_cast<const char*>(&rawCtx), sizeof(uint32_t) + 4);
    auto ss = std::make_shared<std::stringstream>(partialData, std::ios::binary | std::ios::in);
    auto reader = std::make_shared<MinidumpMemoryReader>(ss);
    MinidumpContext ctx(reader);
    EXPECT_FALSE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_EQ(ctx.GetLastError().GetError(), MinidumpError::ERROR_CONTEXT_READ);
}

/**
 * @tc.name: ContextPrintNoARM64ContextTest001
 * @tc.desc: test MinidumpContext Print with non-ARM64 context flags
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpContextTest, ContextPrintNoARM64ContextTest001, TestSize.Level2)
{
    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_INTEGER_FLAG;
    std::string data(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));
    auto reader = MakeReader(data);
    MinidumpContext ctx(reader);
    EXPECT_TRUE(ctx.Read(sizeof(MDRawContextARM64)));
    EXPECT_TRUE(ctx.Valid());
    ctx.Print();
}

} // namespace HiviewDFX
} // namespace OHOS
