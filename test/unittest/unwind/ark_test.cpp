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
const char ARK_LIB_NAME[] = "libark_jsruntime.so";

void* g_handle = nullptr;
pthread_mutex_t g_mutex;
int (*g_stepArkFn)(void*, OHOS::HiviewDFX::ReadMemFunc, OHOS::HiviewDFX::ArkStepParam*);
int (*g_stepArkWithJitFn)(OHOS::HiviewDFX::ArkUnwindParam*);
int (*g_jitCodeWriteFileFn)(void*, OHOS::HiviewDFX::ReadMemFunc, int, const uintptr_t* const, const size_t);
int (*g_parseArkFileInfoFn)(uintptr_t, uintptr_t, const char*, uintptr_t, JsFunction*);
int (*g_parseArkFrameInfoLocalFn)(uintptr_t, uintptr_t, uintptr_t, JsFunction*);
int (*g_parseArkFrameInfoFn)(uintptr_t, uintptr_t, uintptr_t, uint8_t*, uint64_t, uintptr_t, JsFunction*);
int (*g_arkCreateJsSymbolExtractorFn)(uintptr_t*);
int (*g_arkDestoryJsSymbolExtractorFn)(uintptr_t);
int (*g_arkCreateLocalFn)();
int (*g_arkDestroyLocalFn)();
using RustDemangleFn = char*(*)(const char *);
RustDemangleFn g_rustDemangleFn = nullptr;

bool GetLibArkHandle()
{
    if (g_handle != nullptr) {
        return true;
    }
    g_handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
    if (g_handle == nullptr) {
        DFXLOGU("Failed to load library(%{public}s).", dlerror());
        return false;
    }
    return true;
}
} // namespace

#define DLSYM_ARK_FUNC(FuncName, DlsymFuncName) { \
    pthread_mutex_lock(&g_mutex); \
    do { \
        if (!GetLibArkHandle()) { \
            break; \
        } \
        *reinterpret_cast<void**>(&(DlsymFuncName)) = dlsym(g_handle, (FuncName)); \
    } while (false); \
    pthread_mutex_unlock(&g_mutex); \
}

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
    const char* arkFuncName = "ark_create_js_symbol_extractor";
    DLSYM_ARK_FUNC(arkFuncName, g_arkCreateJsSymbolExtractorFn)
    ASSERT_NE(g_arkCreateJsSymbolExtractorFn, nullptr);
    g_arkCreateJsSymbolExtractorFn(extractorPtr);
    g_arkCreateJsSymbolExtractorFn = nullptr;
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
    const char* arkFuncName = "ark_destory_js_symbol_extractor";
    DLSYM_ARK_FUNC(arkFuncName, g_arkDestoryJsSymbolExtractorFn)
    ASSERT_NE(g_arkDestoryJsSymbolExtractorFn, nullptr);
    g_arkDestoryJsSymbolExtractorFn(extractorPtr);
    g_arkDestoryJsSymbolExtractorFn = nullptr;
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
    uintptr_t mapBase = 0;
    const char* name = nullptr;
    uintptr_t extractorPtr = 0;
    JsFunction *jsFunction = nullptr;
    const char* arkFuncName = "ark_parse_js_file_info";
    DLSYM_ARK_FUNC(arkFuncName, g_parseArkFileInfoFn)
    ASSERT_NE(g_parseArkFileInfoFn, nullptr);
    g_parseArkFileInfoFn(byteCodePc, mapBase, name, extractorPtr, jsFunction);
    g_parseArkFileInfoFn = nullptr;
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
    uintptr_t mapBase = 0;
    uintptr_t offset = 0;
    JsFunction *jsFunction = nullptr;
    const char* arkFuncName = "ark_parse_js_frame_info_local";
    DLSYM_ARK_FUNC(arkFuncName, g_parseArkFrameInfoLocalFn)
    ASSERT_NE(g_parseArkFrameInfoLocalFn, nullptr);
    g_parseArkFrameInfoLocalFn(byteCodePc, mapBase, offset, jsFunction);
    g_parseArkFrameInfoLocalFn = nullptr;
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
    uintptr_t mapBase = 0;
    uintptr_t loadOffset = 0;
    uint8_t *data = nullptr;
    uint64_t dataSize = 0;
    uintptr_t extractorPtr = 0;
    JsFunction *jsFunction = nullptr;
    const char* arkFuncName = "ark_parse_js_frame_info";
    DLSYM_ARK_FUNC(arkFuncName, g_parseArkFrameInfoFn)
    ASSERT_NE(g_parseArkFrameInfoFn, nullptr);
    g_parseArkFrameInfoFn(byteCodePc, mapBase, loadOffset, data, dataSize, extractorPtr, jsFunction);
    g_parseArkFrameInfoFn = nullptr;
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
        bool *isJsFrame = nullptr;
        ArkStepParam arkParam(fp, sp, pc, isJsFrame);
        const char* arkFuncName = "step_ark";
        DLSYM_ARK_FUNC(arkFuncName, g_stepArkFn)
        ASSERT_NE(g_stepArkFn, nullptr);
        g_stepArkFn(obj, readMemFn, &arkParam);
        g_stepArkFn = nullptr;
        ASSERT_NE(g_handle, nullptr);
        const char* arkCreateFuncName = "ark_create_local";
        pthread_mutex_lock(&g_mutex);
        *reinterpret_cast<void**>(&(g_arkCreateLocalFn)) = dlsym(g_handle, arkCreateFuncName);
        pthread_mutex_unlock(&g_mutex);
        ASSERT_NE(g_arkCreateLocalFn, nullptr);
        g_arkCreateLocalFn();
        g_arkCreateLocalFn = nullptr;
        const char* arkDestroyFuncName = "ark_destroy_local";
        pthread_mutex_lock(&g_mutex);
        *reinterpret_cast<void**>(&(g_arkDestroyLocalFn)) = dlsym(g_handle, arkDestroyFuncName);
        pthread_mutex_unlock(&g_mutex);
        ASSERT_NE(g_arkDestroyLocalFn, nullptr);
        g_arkDestroyLocalFn();
        g_arkDestroyLocalFn = nullptr;
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
    OHOS::HiviewDFX::ArkUnwindParam arkParam(ctx, readMem, fp, sp, pc, methodId, isJsFrame, jitCache);
    const char* const arkFuncName = "step_ark_with_record_jit";
    DLSYM_ARK_FUNC(arkFuncName, g_stepArkWithJitFn)
    ASSERT_NE(g_stepArkWithJitFn, nullptr);
    g_stepArkWithJitFn(&arkParam);
    g_stepArkWithJitFn = nullptr;
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
    const char* const arkFuncName = "ark_write_jit_code";
    DLSYM_ARK_FUNC(arkFuncName, g_jitCodeWriteFileFn)
    ASSERT_NE(g_jitCodeWriteFileFn, nullptr);
    g_jitCodeWriteFileFn(ctx, readMemFn, fd, jitCodeArray, jitSize);
    g_jitCodeWriteFileFn = nullptr;
    GTEST_LOG_(INFO) << "ArkTest008: end.";
}

/**
 * @tc.name: ArkTest009
 * @tc.desc: test rustc_demangle functions
 * @tc.type: FUNC
 */
HWTEST_F(ArkTest, ArkTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkTest009: start.";
    void* handle = dlopen("librustc_demangle.z.so", RTLD_LAZY | RTLD_NODELETE);
    ASSERT_TRUE(handle) << "Failed to dlopen librustc_demangle";
    g_rustDemangleFn = (RustDemangleFn)dlsym(handle, "rustc_demangle");
    ASSERT_TRUE(g_rustDemangleFn) << "Failed to dlsym rustc_demangle";
    std::string reason = "reason";
    const char *bufStr = reason.c_str();
    g_rustDemangleFn(bufStr);
    g_rustDemangleFn = nullptr;
    GTEST_LOG_(INFO) << "ArkTest009: end.";
}
}
}