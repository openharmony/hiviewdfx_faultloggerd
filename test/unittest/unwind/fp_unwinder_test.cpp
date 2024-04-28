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
#include "fp_unwinder.h"
#include "unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxFpUnwinderTest"
#define LOG_DOMAIN 0xD002D11

class FpUnwinderTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: FpUnwinderTest001
 * @tc.desc: test fp unwinder Unwind interface
 * @tc.type: FUNC
 */
HWTEST_F(FpUnwinderTest, FpUnwinderTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FpUnwinderTest001: start.";
#if defined(__aarch64__)
    uintptr_t regs[2]; // 2: pc and fp reg
    FpUnwinder::GetPcFpRegs(regs);
    uintptr_t pc = regs[0];
    uintptr_t fp = regs[1];
    const size_t maxSize = 32;
    uintptr_t pcs[maxSize] = {0};
    auto unwSz = FpUnwinder::GetPtr()->Unwind(pc, fp, pcs, maxSize);
    ASSERT_GT(unwSz, 1);

    std::vector<DfxFrame> frames;
    for (auto i = 0; i < unwSz; ++i) {
        DfxFrame frame;
        frame.index = i;
        frame.pc = static_cast<uint64_t>(pcs[i]);
        frames.emplace_back(frame);
    }
    ASSERT_GT(frames.size(), 1);
    Unwinder::FillLocalFrames(frames);
    GTEST_LOG_(INFO) << "FpUnwinderTest001: frames:\n" << Unwinder::GetFramesStr(frames);
#endif
    GTEST_LOG_(INFO) << "FpUnwinderTest001: end.";
}

/**
 * @tc.name: FpUnwinderTest002
 * @tc.desc: test fp unwinder UnwindSafe interface
 * @tc.type: FUNC
 */
HWTEST_F(FpUnwinderTest, FpUnwinderTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FpUnwinderTest002: start.";
#if defined(__aarch64__)
    uintptr_t regs[2]; // 2: pc and fp reg
    FpUnwinder::GetPcFpRegs(regs);
    uintptr_t pc = regs[0];
    uintptr_t fp = regs[1];
    const size_t maxSize = 32;
    uintptr_t pcs[maxSize] = {0};
    auto unwSz = FpUnwinder::GetPtr()->UnwindSafe(pc, fp, pcs, maxSize);
    ASSERT_GT(unwSz, 1);

    std::vector<DfxFrame> frames;
    for (auto i = 0; i < unwSz; ++i) {
        DfxFrame frame;
        frame.index = i;
        frame.pc = static_cast<uint64_t>(pcs[i]);
        frames.emplace_back(frame);
    }
    ASSERT_GT(frames.size(), 1);
    Unwinder::FillLocalFrames(frames);
    GTEST_LOG_(INFO) << "FpUnwinderTest002: frames:\n" << Unwinder::GetFramesStr(frames);
#endif
    GTEST_LOG_(INFO) << "FpUnwinderTest002: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS