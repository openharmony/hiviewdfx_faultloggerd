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

#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>

#include "dfx_jsvm.h"
#include "dfx_log.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
bool g_jsvmEnable = false;
bool g_arkwebjsEnable = false;

class JsvmTest : public testing::Test {
public:
    void SetUp() override
    {
        std::string filePath;
#if defined(__arm__)
        filePath = "/system/lib/ndk/libjsvm.so";
#elif defined(__aarch64__)
        filePath = "/system/lib64/ndk/libjsvm.so";
#endif
        g_jsvmEnable = filesystem::exists(filePath);
#if defined(__arm__)
        filePath = "/data/app/el1/bundle/public/com.huawei.hmos.arkwebcore/libs/arm/libwebjs_dfx.so";
#elif defined(__aarch64__)
        filePath = "/data/app/el1/bundle/public/com.huawei.hmos.arkwebcore/libs/arm64/libwebjs_dfx.so";
#endif
        g_arkwebjsEnable = filesystem::exists(filePath);
    }
};

/**
 * @tc.name: JsvmTest001
 * @tc.desc: test JsvmCreateJsSymbolExtractor function
 * @tc.type: FUNC
 */
HWTEST_F(JsvmTest, JsvmTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "JsvmTest001: start.";
    uintptr_t extractorPtr = 0;
    if (g_jsvmEnable) {
        DfxJsvm::Instance().JsvmCreateJsSymbolExtractor(&extractorPtr, getpid());
        ASSERT_NE(DfxJsvm::Instance().jsvmCreateJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().JsvmCreateJsSymbolExtractor(&extractorPtr, getpid());
        ASSERT_NE(DfxJsvm::Instance().jsvmCreateJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().JsvmDestroyJsSymbolExtractor(extractorPtr);
        ASSERT_NE(DfxJsvm::Instance().jsvmDestroyJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().JsvmDestroyJsSymbolExtractor(extractorPtr);
        ASSERT_NE(DfxJsvm::Instance().jsvmDestroyJsSymbolExtractorFn_, nullptr);
    } else {
        DfxJsvm::Instance().JsvmCreateJsSymbolExtractor(&extractorPtr, getpid());
        ASSERT_EQ(DfxJsvm::Instance().jsvmCreateJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().JsvmDestroyJsSymbolExtractor(extractorPtr);
        ASSERT_EQ(DfxJsvm::Instance().jsvmDestroyJsSymbolExtractorFn_, nullptr);
    }
    GTEST_LOG_(INFO) << "JsvmTest001: end.";
}

/**
 * @tc.name: JsvmTest002
 * @tc.desc: test ParseJsvmFrameInfo function
 * @tc.type: FUNC
 */
HWTEST_F(JsvmTest, JsvmTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "JsvmTest002: start.";
    uintptr_t pc = 0;
    uintptr_t extractorPtr = 0;
    JsvmFunction *jsFunction = nullptr;
    if (g_jsvmEnable) {
        DfxJsvm::Instance().ParseJsvmFrameInfo(pc, extractorPtr, jsFunction);
        ASSERT_NE(DfxJsvm::Instance().parseJsvmFrameInfoFn_, nullptr);
        DfxJsvm::Instance().ParseJsvmFrameInfo(pc, extractorPtr, jsFunction);
        ASSERT_NE(DfxJsvm::Instance().parseJsvmFrameInfoFn_, nullptr);
    } else {
        DfxJsvm::Instance().ParseJsvmFrameInfo(pc, extractorPtr, jsFunction);
        ASSERT_EQ(DfxJsvm::Instance().parseJsvmFrameInfoFn_, nullptr);
    }

    GTEST_LOG_(INFO) << "JsvmTest002: end.";
}

/**
 * @tc.name: JsvmTest003
 * @tc.desc: test StepJsvmFrame function
 * @tc.type: FUNC
 */
HWTEST_F(JsvmTest, JsvmTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "JsvmTest003: start.";
    void *obj = nullptr;
    ReadMemFunc readMemFn = nullptr;
    JsvmStepParam *jsvmParam = nullptr;
    DfxJsvm::Instance().StepJsvmFrame(obj, readMemFn, jsvmParam);
    int a = 0;
    obj = &a;
    DfxJsvm::Instance().StepJsvmFrame(obj, readMemFn, jsvmParam);
    ASSERT_EQ(DfxJsvm::Instance().stepJsvmFn_, nullptr);
    GTEST_LOG_(INFO) << "JsvmTest003: end.";
}

/**
 * @tc.name: ArkwebJsTest001
 * @tc.desc: test ArkwebCreateJsSymbolExtractor function
 * @tc.type: FUNC
 */
HWTEST_F(JsvmTest, ArkwebJsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkwebJsTest001: start.";
    uintptr_t extractorPtr = 0;
    if (g_jsvmEnable) {
        DfxJsvm::Instance().ArkwebCreateJsSymbolExtractor(&extractorPtr, getpid());
        ASSERT_NE(DfxJsvm::Instance().arkwebCreateJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().ArkwebCreateJsSymbolExtractor(&extractorPtr, getpid());
        ASSERT_NE(DfxJsvm::Instance().arkwebCreateJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().ArkwebDestroyJsSymbolExtractor(extractorPtr);
        ASSERT_NE(DfxJsvm::Instance().arkwebDestroyJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().ArkwebDestroyJsSymbolExtractor(extractorPtr);
        ASSERT_NE(DfxJsvm::Instance().arkwebDestroyJsSymbolExtractorFn_, nullptr);
    } else {
        DfxJsvm::Instance().ArkwebCreateJsSymbolExtractor(&extractorPtr, getpid());
        ASSERT_EQ(DfxJsvm::Instance().arkwebCreateJsSymbolExtractorFn_, nullptr);
        DfxJsvm::Instance().ArkwebDestroyJsSymbolExtractor(extractorPtr);
        ASSERT_EQ(DfxJsvm::Instance().arkwebDestroyJsSymbolExtractorFn_, nullptr);
    }
    GTEST_LOG_(INFO) << "ArkwebJsTest001: end.";
}

/**
 * @tc.name: ArkwebJsTest002
 * @tc.desc: test ParseArkwebJsFrameInfo function
 * @tc.type: FUNC
 */
HWTEST_F(JsvmTest, ArkwebJsTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkwebJsTest002: start.";
    uintptr_t pc = 0;
    uintptr_t extractorPtr = 0;
    JsvmFunction *jsFunction = nullptr;
    if (g_jsvmEnable) {
        DfxJsvm::Instance().ParseArkwebJsFrameInfo(pc, extractorPtr, jsFunction);
        ASSERT_NE(DfxJsvm::Instance().parseArkwebJsFrameInfoFn_, nullptr);
        DfxJsvm::Instance().ParseArkwebJsFrameInfo(pc, extractorPtr, jsFunction);
        ASSERT_NE(DfxJsvm::Instance().parseArkwebJsFrameInfoFn_, nullptr);
    } else {
        DfxJsvm::Instance().ParseArkwebJsFrameInfo(pc, extractorPtr, jsFunction);
        ASSERT_EQ(DfxJsvm::Instance().parseArkwebJsFrameInfoFn_, nullptr);
    }

    GTEST_LOG_(INFO) << "ArkwebJsTest002: end.";
}

/**
 * @tc.name: ArkwebJsTest003
 * @tc.desc: test StepArkwebJsFrame function
 * @tc.type: FUNC
 */
HWTEST_F(JsvmTest, ArkwebJsTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArkwebJsTest003: start.";
    void *obj = nullptr;
    ReadMemFunc readMemFn = nullptr;
    JsvmStepParam *jsvmParam = nullptr;
    DfxJsvm::Instance().StepArkwebJsFrame(obj, readMemFn, jsvmParam);
    int a = 0;
    obj = &a;
    DfxJsvm::Instance().StepArkwebJsFrame(obj, readMemFn, jsvmParam);
    ASSERT_EQ(DfxJsvm::Instance().stepArkwebJsFn_, nullptr);
    GTEST_LOG_(INFO) << "ArkwebJsTest003: end.";
}
}
}