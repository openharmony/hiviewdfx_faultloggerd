/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include <pthread.h>

#include "lock_parser.h"
#include "unwinder.h"
#include "unwinder_config.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class LockParserUnittest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};
} // namespace HiviewDFX
} // namespace OHOS

void LockParserUnittest::SetUpTestCase(void)
{
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
}

void LockParserUnittest::TearDownTestCase(void)
{
}

void LockParserUnittest::SetUp(void)
{
}

void LockParserUnittest::TearDown(void)
{
}

namespace {
constexpr const int LOCK_TYPE_IDX = 0;
constexpr const int LOCK_OWNER_IDX = 1;
constexpr const int LOCK_OWNER_MASK = 0x3fffffff;
void InitMutexByType(int32_t type, pthread_mutex_t& mutex)
{
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, type);

    pthread_mutex_init(&mutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);
}

void LockMutex(pthread_mutex_t& mutex)
{
    auto mutexInt = reinterpret_cast<int*>(&mutex);
    printf("mutex address:%llx\n", reinterpret_cast<long long>(&mutex));
    printf("mutex owner before lock:%d\n", mutexInt[LOCK_OWNER_IDX]);
    pthread_mutex_lock(&mutex);
    printf("mutex owner after lock:%d\n", mutexInt[LOCK_OWNER_IDX]);
}

void WaitThreadBlock(int& tid)
{
    while (true) {
        if (tid > 0) {
            printf("WaitThreadBlock:%d\n", tid);
            break;
        }
        sleep(1);
    }
}

/**
 * @tc.name: LockParserUnittest001
 * @tc.desc: unwinder parse errorcheck lock owner
 * @tc.type: FUNC
 */
HWTEST_F(LockParserUnittest, LockParserUnittest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "LockParserUnittest001: start.";
    pthread_mutex_t mutex;
    InitMutexByType(PTHREAD_MUTEX_ERRORCHECK, mutex);

    auto mutexInt = reinterpret_cast<int*>(&mutex);
    LockMutex(mutex);

    int tid = 0;
    std::thread t1([&tid, &mutex] {
        tid = gettid();
        printf("BlockTid:%d\n", tid);
        pthread_mutex_lock(&mutex);
    });

    WaitThreadBlock(tid);
    printf("CurrentTid:%d BlockTid:%d\n", gettid(), tid);
    auto unwinder = std::make_shared<Unwinder>(true);
    ASSERT_EQ(unwinder->UnwindLocalWithTid(tid), true);

    std::vector<uintptr_t> pcs = unwinder->GetPcs();
    ASSERT_FALSE(pcs.empty());

    std::vector<DfxFrame> frames;
    (void)unwinder->GetFramesByPcs(frames, pcs);
    ASSERT_FALSE(frames.empty());
    unwinder->SetFrames(frames);

    std::vector<char> buffer(sizeof(pthread_mutex_t), 0);
    if (!unwinder->GetLockInfo(tid, buffer.data(), sizeof(pthread_mutex_t))) {
        pthread_mutex_unlock(&mutex);
        ASSERT_TRUE(false);
    }

    if (memcmp(buffer.data(), &mutex, sizeof(pthread_mutex_t)) != 0) {
        pthread_mutex_unlock(&mutex);
        ASSERT_TRUE(false);
    }

    int lockOwner = mutexInt[LOCK_OWNER_IDX] & LOCK_OWNER_MASK;
    ASSERT_EQ(gettid(), lockOwner);
    printf("CurrentTid:%d Lock owner:%d\n", gettid(), lockOwner);
    ASSERT_EQ(PTHREAD_MUTEX_ERRORCHECK, mutexInt[LOCK_TYPE_IDX]);
    pthread_mutex_unlock(&mutex);
    if (t1.joinable()) {
        t1.join();
    }
    GTEST_LOG_(INFO) << "LockParserUnittest001: end.";
}

/**
 * @tc.name: LockParserUnittest002
 * @tc.desc: unwinder parse normal lock owner
 * @tc.type: FUNC
 */
HWTEST_F(LockParserUnittest, LockParserUnittest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LockParserUnittest002: start.";
    pthread_mutex_t mutex;
    InitMutexByType(PTHREAD_MUTEX_NORMAL, mutex);

    auto mutexInt = reinterpret_cast<int*>(&mutex);
    LockMutex(mutex);

    int tid = 0;
    std::thread t1([&tid, &mutex] {
        tid = gettid();
        printf("BlockTid:%d\n", tid);
        pthread_mutex_lock(&mutex);
    });

    WaitThreadBlock(tid);
    printf("CurrentTid:%d BlockTid:%d\n", gettid(), tid);
    auto unwinder = std::make_shared<Unwinder>(true);
    ASSERT_EQ(unwinder->UnwindLocalWithTid(tid), true);

    std::vector<uintptr_t> pcs = unwinder->GetPcs();
    ASSERT_FALSE(pcs.empty());

    std::vector<DfxFrame> frames;
    (void)unwinder->GetFramesByPcs(frames, pcs);
    ASSERT_FALSE(frames.empty());
    unwinder->SetFrames(frames);

    std::vector<char> buffer(sizeof(pthread_mutex_t), 0);
    if (!unwinder->GetLockInfo(tid, buffer.data(), sizeof(pthread_mutex_t))) {
        pthread_mutex_unlock(&mutex);
        ASSERT_TRUE(false);
    }

    if (memcmp(buffer.data(), &mutex, sizeof(pthread_mutex_t)) != 0) {
        pthread_mutex_unlock(&mutex);
        ASSERT_TRUE(false);
    }

    int lockOwner = mutexInt[LOCK_OWNER_IDX] & LOCK_OWNER_MASK;
    ASSERT_EQ(EBUSY, lockOwner);
    printf("EBUSY:%d Lock owner:%d\n", EBUSY, lockOwner);
    ASSERT_EQ(PTHREAD_MUTEX_NORMAL, mutexInt[LOCK_TYPE_IDX]);
    pthread_mutex_unlock(&mutex);
    if (t1.joinable()) {
        t1.join();
    }
    GTEST_LOG_(INFO) << "LockParserUnittest002: end.";
}

/**
 * @tc.name: LockParserUnittest003
 * @tc.desc: test lock parser parse normal lock
 * @tc.type: FUNC
 */
HWTEST_F(LockParserUnittest, LockParserUnittest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LockParserUnittest003: start.";
    pthread_mutex_t mutex;
    InitMutexByType(PTHREAD_MUTEX_NORMAL, mutex);
    LockMutex(mutex);

    int tid = 0;
    std::thread t1([&tid, &mutex] {
        tid = gettid();
        printf("BlockTid:%d\n", tid);
        pthread_mutex_lock(&mutex);
    });

    WaitThreadBlock(tid);
    printf("CurrentTid:%d BlockTid:%d\n", gettid(), tid);
    Unwinder unwinder;
    ASSERT_EQ(unwinder.UnwindLocalWithTid(tid), true);

    std::vector<uintptr_t> pcs = unwinder.GetPcs();
    std::vector<DfxFrame> frames;
    (void)unwinder.GetFramesByPcs(frames, pcs);
    unwinder.SetFrames(frames);

    bool ret = LockParser::ParseLockInfo(unwinder, getpid(), tid);
    ASSERT_EQ(ret, true);

    pthread_mutex_unlock(&mutex);
    if (t1.joinable()) {
        t1.join();
    }
    GTEST_LOG_(INFO) << "LockParserUnittest003: end.";
}

/**
 * @tc.name: LockParserUnittest004
 * @tc.desc: test lock parser parse errorcheck lock
 * @tc.type: FUNC
 */
HWTEST_F(LockParserUnittest, LockParserUnittest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LockParserUnittest004: start.";
    pthread_mutex_t mutex;
    InitMutexByType(PTHREAD_MUTEX_ERRORCHECK, mutex);
    LockMutex(mutex);

    int tid = 0;
    std::thread t1([&tid, &mutex] {
        tid = gettid();
        printf("BlockTid:%d\n", tid);
        pthread_mutex_lock(&mutex);
    });

    WaitThreadBlock(tid);
    printf("CurrentTid:%d BlockTid:%d\n", gettid(), tid);
    Unwinder unwinder;
    ASSERT_EQ(unwinder.UnwindLocalWithTid(tid), true);

    std::vector<uintptr_t> pcs = unwinder.GetPcs();
    std::vector<DfxFrame> frames;
    (void)unwinder.GetFramesByPcs(frames, pcs);
    unwinder.SetFrames(frames);

    bool ret = LockParser::ParseLockInfo(unwinder, getpid(), tid);
    ASSERT_EQ(ret, true);

    pthread_mutex_unlock(&mutex);
    if (t1.joinable()) {
        t1.join();
    }
    GTEST_LOG_(INFO) << "LockParserUnittest004: end.";
}

/**
 * @tc.name: LockParserUnittest005
 * @tc.desc: test lock parser parse PTHREAD_MUTEX_RECURSIVE lock
 * @tc.type: FUNC
 */
HWTEST_F(LockParserUnittest, LockParserUnittest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LockParserUnittest005: start.";
    pthread_mutex_t mutex;
    InitMutexByType(PTHREAD_MUTEX_RECURSIVE, mutex);
    LockMutex(mutex);

    int tid = 0;
    std::thread t1([&tid, &mutex] {
        tid = gettid();
        printf("BlockTid:%d\n", tid);
        pthread_mutex_lock(&mutex);
    });

    WaitThreadBlock(tid);
    printf("CurrentTid:%d BlockTid:%d\n", gettid(), tid);
    Unwinder unwinder;
    ASSERT_EQ(unwinder.UnwindLocalWithTid(tid), true);

    std::vector<uintptr_t> pcs = unwinder.GetPcs();
    ASSERT_FALSE(pcs.empty());

    std::vector<DfxFrame> frames;
    (void)unwinder.GetFramesByPcs(frames, pcs);
    ASSERT_FALSE(frames.empty());
    unwinder.SetFrames(frames);

    std::vector<char> buffer(sizeof(pthread_mutex_t), 0);
    if (!unwinder.GetLockInfo(tid, buffer.data(), sizeof(pthread_mutex_t))) {
        pthread_mutex_unlock(&mutex);
        ASSERT_TRUE(false);
    }

    if (memcmp(buffer.data(), &mutex, sizeof(pthread_mutex_t)) != 0) {
        pthread_mutex_unlock(&mutex);
        ASSERT_TRUE(false);
    }

    auto mutexInt = reinterpret_cast<int*>(&mutex);
    int lockOwner = mutexInt[LOCK_OWNER_IDX] & LOCK_OWNER_MASK;
    ASSERT_EQ(gettid(), lockOwner);
    printf("CurrentTid:%d Lock owner:%d\n", gettid(), lockOwner);
    ASSERT_EQ(PTHREAD_MUTEX_RECURSIVE, mutexInt[LOCK_TYPE_IDX]);
    ASSERT_EQ(LockParser::ParseLockInfo(unwinder, getpid(), tid), true);

    pthread_mutex_unlock(&mutex);
    if (t1.joinable()) {
        t1.join();
    }
    GTEST_LOG_(INFO) << "LockParserUnittest005: end.";
}
}