/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <map>
#include <thread>
#include <unistd.h>
#include <hilog/log.h>
#include <malloc.h>
#include <securec.h>
#include "dfx_ptrace.h"
#include "unwinder.h"
#include "elapsed_time.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxUnwinderTest"
#define LOG_DOMAIN 0xD002D11

class UnwinderTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}

    std::map<int, std::shared_ptr<Unwinder>> unwinders_;
};

/**
 * @tc.name: UnwinderLocalTest001
 * @tc.desc: test unwinder local unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderLocalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: start.";
    pid_t child = fork();
    if (child == 0) {
        auto unwinder = std::make_shared<Unwinder>();
        unwinder->SetLocalMainThread(true);
        ElapsedTime counter;
        bool unwRet = unwinder->UnwindLocal();
        EXPECT_EQ(true, unwRet) << "UnwinderLocalTest001: Unwind:" << unwRet;
        GTEST_LOG_(INFO) << "Elapsed-: " << counter.Elapsed();
        std::vector<DfxFrame> frames;
        Unwinder::GetFramesByPcs(frames, unwinder->GetPcs(), unwinder->GetMaps());
        ASSERT_GT(frames.size(), 0);
        GTEST_LOG_(INFO) << "Elapsed+: " << counter.Elapsed();
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: end.";
}

/**
 * @tc.name: UnwinderLocalTest002
 * @tc.desc: test unwinder local unwind n counts
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderLocalTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderLocalTest002: start.";
    pid_t child = fork();
    if (child == 0) {
        unwinders_.clear();
        std::shared_ptr<Unwinder> unwinder = nullptr;
        pid_t pid = getpid();
        GTEST_LOG_(INFO) << "pid: " << pid;
        for (int i = 0; i < 10; ++i) {
            auto it = unwinders_.find(pid);
            if (it != unwinders_.end()) {
                unwinder = it->second;
            } else {
                unwinder = std::make_shared<Unwinder>();
                unwinder->SetLocalMainThread(true);
            }
            ElapsedTime counter;
            bool unwRet = unwinder->UnwindLocal();
            EXPECT_EQ(true, unwRet) << "UnwinderLocalTest002: Unwind:" << unwRet;
            GTEST_LOG_(INFO) << "Elapsed-: " << counter.Elapsed();
            std::vector<DfxFrame> frames;
            Unwinder::GetFramesByPcs(frames, unwinder->GetPcs(), unwinder->GetMaps());
            ASSERT_GT(frames.size(), 0);
            GTEST_LOG_(INFO) << "Elapsed+: " << counter.Elapsed();
            GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
            unwinders_[pid] = unwinder;
            sleep(5);
        }
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderLocalTest002: end.";
}

/**
 * @tc.name: UnwinderRemoteTest001
 * @tc.desc: test unwinder remote unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderRemoteTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: Attach:" << unwRet;
        ElapsedTime counter;
        unwRet = unwinder->UnwindRemote();
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: Unwind:" << unwRet;
        GTEST_LOG_(INFO) << "Elapsed-: " << counter.Elapsed();
        std::vector<DfxFrame> frames;
        Unwinder::GetFramesByPcs(frames, unwinder->GetPcs(), unwinder->GetMaps());
        ASSERT_GT(frames.size(), 0);
        GTEST_LOG_(INFO) << "Elapsed+: " << counter.Elapsed();
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: end.";
}

/**
 * @tc.name: UnwinderRemoteTest002
 * @tc.desc: test unwinder remote unwind n counts
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderRemoteTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderRemoteTest002: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        unwinders_.clear();
        std::shared_ptr<Unwinder> unwinder = nullptr;
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest002: Attach:" << unwRet;
        for (int i = 0; i < 10; ++i) {
            auto it = unwinders_.find(pid);
            if (it != unwinders_.end()) {
                unwinder = it->second;
            } else {
                unwinder = std::make_shared<Unwinder>(pid);
            }
            ElapsedTime counter;
            unwRet = unwinder->UnwindRemote();
            EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest002: Unwind:" << unwRet;
            GTEST_LOG_(INFO) << "Elapsed-: " << counter.Elapsed();
            std::vector<DfxFrame> frames;
            Unwinder::GetFramesByPcs(frames, unwinder->GetPcs(), unwinder->GetMaps());
            ASSERT_GT(frames.size(), 0);
            GTEST_LOG_(INFO) << "Elapsed+: " << counter.Elapsed();
            GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
            unwinders_[pid] = unwinder;
            sleep(5);
        }
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderRemoteTest002: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
