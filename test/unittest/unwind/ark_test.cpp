/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "dfx_ark.h"
#include "dfx_log.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace {
using RustDemangleFn = char*(*)(const char *);
RustDemangleFn g_rustDemangleFn = nullptr;
} // namespace

class ArkTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: ArkTest001
 * @tc.desc: test ArkCreateJsSymbolExtractor functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest001: start.";
    uintptr_t zero = 0;
    uintptr_t* extractorPtr = &zero;
    DfxArk::Instance().ArkCreateJsSymbolExtractor(extractorPtr);
    ASSERT_NE(DfxArk::Instance().arkCreateJsSymbolExtractorFn_, nullptr);
    DfxArk::Instance().arkCreateJsSymbolExtractorFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest001: end.";
}

/**
 * @tc.name: ArkTest002
 * @tc.desc: test ArkDestoryJsSymbolExtractor functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest002: start.";
    uintptr_t extractorPtr = 0;
    DfxArk::Instance().ArkDestoryJsSymbolExtractor(extractorPtr);
    ASSERT_NE(DfxArk::Instance().arkDestoryJsSymbolExtractorFn_, nullptr);
    DfxArk::Instance().arkDestoryJsSymbolExtractorFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest002: end.";
}

/**
 * @tc.name: ArkTest003
 * @tc.desc: test ParseArkFileInfo functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest003: start.";
    uintptr_t byteCodePc = 0;
    uintptr_t methodid = 0;
    uintptr_t mapBase = 0;
    const char* name = nullptr;
    uintptr_t extractorPtr = 0;
    JsFunction *jsFunction = nullptr;
    DfxArk::Instance().ParseArkFileInfo(byteCodePc, methodid, mapBase, name, extractorPtr, jsFunction);
    ASSERT_NE(DfxArk::Instance().parseArkFileInfoFn_, nullptr);
    DfxArk::Instance().parseArkFileInfoFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest003: end.";
}

/**
 * @tc.name: ArkTest004
 * @tc.desc: test ParseArkFrameInfoLocal functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest004: start.";
    uintptr_t byteCodePc = 0;
    uintptr_t methodid = 0;
    uintptr_t mapBase = 0;
    uintptr_t offset = 0;
    JsFunction *jsFunction = nullptr;
    DfxArk::Instance().ParseArkFrameInfoLocal(byteCodePc, methodid, mapBase, offset, jsFunction);
    ASSERT_NE(DfxArk::Instance().parseArkFrameInfoLocalFn_, nullptr);
    DfxArk::Instance().parseArkFrameInfoLocalFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest004: end.";
}

/**
 * @tc.name: ArkTest005
 * @tc.desc: test ParseArkFrameInfo functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest005: start.";
    uintptr_t byteCodePc = 0;
    uintptr_t methodid = 0;
    uintptr_t mapBase = 0;
    uintptr_t loadOffset = 0;
    uint8_t *data = nullptr;
    uint64_t dataSize = 0;
    uintptr_t extractorPtr = 0;
    JsFunction *jsFunction = nullptr;
    DfxArk::Instance().ParseArkFrameInfo(byteCodePc, methodid, mapBase, loadOffset,
        data, dataSize, extractorPtr, jsFunction);
    ASSERT_NE(DfxArk::Instance().parseArkFrameInfoFn_, nullptr);
    DfxArk::Instance().parseArkFrameInfoFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest005: end.";
}

/**
 * @tc.name: ArkTest006
 * @tc.desc: test StepArkFrame functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest006: start.";
    pid_t pid = fork();
    if (pid == 0) {
        uintptr_t zero = 0;
        void *obj = nullptr;
        OHOS::HiviewDFX::ReadMemFunc readMemFn = nullptr;
        uintptr_t *fp = &zero;
        uintptr_t *sp = &zero;
        uintptr_t *pc = &zero;
        uintptr_t* methodid = &zero;
        bool *isJsFrame = nullptr;
        DfxArk::Instance().StepArkFrame(obj, readMemFn, fp, sp, pc, methodid, isJsFrame);
        ASSERT_NE(DfxArk::Instance().stepArkFn_, nullptr);
        DfxArk::Instance().stepArkFn_ = nullptr;
        ASSERT_NE(DfxArk::Instance().handle_, nullptr);
        const char* arkCreateLocalFn = "ark_create_local";
        std::unique_lock<std::mutex> lock(DfxArk::Instance().arkMutex_);
        *reinterpret_cast<void**>(&(DfxArk::Instance().arkCreateLocalFn_)) = dlsym(DfxArk::Instance().handle_,
                                                                                   arkCreateLocalFn);
        ASSERT_NE(DfxArk::Instance().arkCreateLocalFn_, nullptr);
        DfxArk::Instance().arkCreateLocalFn_();
        DfxArk::Instance().arkCreateLocalFn_ = nullptr;
        const char* arkDestroyFuncName = "ark_destroy_local";
        *reinterpret_cast<void**>(&(DfxArk::Instance().arkDestroyLocalFn_)) = dlsym(DfxArk::Instance().handle_,
                                                                                   arkDestroyFuncName);
        ASSERT_NE(DfxArk::Instance().arkDestroyLocalFn_, nullptr);
        DfxArk::Instance().arkDestroyLocalFn_();
        DfxArk::Instance().arkDestroyLocalFn_ = nullptr;
        exit(0);
    }
    int status;
    bool isSuccess = waitpid(pid, &status, 0) != -1;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }

    int exitCode = -1;
    if (WIFEXITED(status)) {
        exitCode = WEXITSTATUS(status);
        printf("Exit status was %d\n", exitCode);
    }
    ASSERT_EQ(exitCode, 0);
    GTEST_LOG_(INFO) << "ArkTest006: end.";
}

/**
 * @tc.name: ArkTest007
 * @tc.desc: test StepArkFrameWithJit functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest007: start.";
    uintptr_t zero = 0;
    void *ctx = &zero;
    ReadMemFunc readMem = nullptr;
    uintptr_t *fp = &zero;
    uintptr_t *sp = &zero;
    uintptr_t *pc = &zero;
    uintptr_t *methodId = &zero;
    bool *isJsFrame = nullptr;
    std::vector<uintptr_t> vec;
    std::vector<uintptr_t>& jitCache = vec;
    OHOS::HiviewDFX::ArkUnwindParam ark(ctx, readMem, fp, sp, pc, methodId, isJsFrame, jitCache);
    OHOS::HiviewDFX::ArkUnwindParam* arkPrama = &ark;
    DfxArk::Instance().StepArkFrameWithJit(arkPrama);
    ASSERT_NE(DfxArk::Instance().stepArkWithJitFn_, nullptr);
    DfxArk::Instance().stepArkWithJitFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest007: end.";
}

/**
 * @tc.name: ArkTest008
 * @tc.desc: test JitCodeWriteFile functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest008: start.";
    void* ctx = nullptr;
    OHOS::HiviewDFX::ReadMemFunc readMemFn = nullptr;
    int fd = -1;
    const uintptr_t* const jitCodeArray = nullptr;
    const size_t jitSize = 0;
    DfxArk::Instance().JitCodeWriteFile(ctx, readMemFn, fd, jitCodeArray, jitSize);
    ASSERT_NE(DfxArk::Instance().jitCodeWriteFileFn_, nullptr);
    DfxArk::Instance().jitCodeWriteFileFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest008: end.";
}

/**
 * @tc.name: ArkTest009
 * @tc.desc: test GetArkNativeFrameInfo functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest009: start.";
    int pid = 0;
    uintptr_t zero = 0;
    uintptr_t& pc = zero;
    uintptr_t& fp = zero;
    uintptr_t& sp = zero;
    JsFrame* frames = nullptr;
    size_t& size = zero;
    DfxArk::Instance().GetArkNativeFrameInfo(pid, pc, fp, sp, frames, size);
    ASSERT_NE(DfxArk::Instance().getArkNativeFrameInfoFn_, nullptr);
    DfxArk::Instance().getArkNativeFrameInfoFn_ = nullptr;
    GTEST_LOG_(INFO) << "ArkTest009: end.";
}

/**
 * @tc.name: ArkTest010
 * @tc.desc: test rustc_demangle functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest010: start.";
    void* handle = dlopen("librustc_demangle.z.so", RTLD_LAZY | RTLD_NODELETE);
    ASSERT_TRUE(handle) << "Failed to dlopen librustc_demangle";
    g_rustDemangleFn = (RustDemangleFn)dlsym(handle, "rustc_demangle");
    ASSERT_TRUE(g_rustDemangleFn) << "Failed to dlsym rustc_demangle";
    std::string reason = "reason";
    const char *bufStr = reason.c_str();
    g_rustDemangleFn(bufStr);
    g_rustDemangleFn = nullptr;
    GTEST_LOG_(INFO) << "ArkTest010: end.";
}
}
}