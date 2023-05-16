/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <sigchain.h>

#include "dfx_define.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class SignalChainTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void SignalChainTest::SetUpTestCase()
{}

void SignalChainTest::TearDownTestCase()
{}

void SignalChainTest::SetUp()
{}

void SignalChainTest::TearDown()
{}

#define SIGCHIAN_TEST_SIGNAL_NUM_1 1
#define SIGCHIAN_TEST_SIGNAL_NUM_2 2

static const int TEST_PTR_VALUE = 10;
static const int SLEEP_10_MS = 10000;
static const int SLEEP_1000_MS = 1000000;
static const int SLEEP_2000_MS = 1000000;

static int g_count = 0;
static bool g_testLastFlag = false;
static bool g_signalDumpFlag = false;
static bool g_signalSegvFlag = false;
static bool g_sigactionDumpFlag = false;
static bool g_sigactionSegvFlag = false;
static bool g_sigactionIllFlag = false;
static bool g_sigchainDumpFlag = false;
static bool g_sigchainDump1Flag = false;
static bool g_sigchainDump2Flag = false;
static bool g_sigchainSegvFlag = false;
static bool g_sigchainSegv1Flag = false;
static bool g_sigchainSegv2Flag = false;

static void ResetCount()
{
    g_count = 0;
    g_testLastFlag = true;
}

static void SignalInit()
{
    g_testLastFlag = false;
    g_signalDumpFlag = false;
    g_signalSegvFlag = false;
    g_sigactionDumpFlag = false;
    g_sigactionSegvFlag = false;
    g_sigactionIllFlag = false;
    g_sigchainDumpFlag = false;
    g_sigchainDump1Flag = false;
    g_sigchainDump2Flag = false;
    g_sigchainSegvFlag = false;
    g_sigchainSegv1Flag = false;
    g_sigchainSegv2Flag = false;
}

ATTRIBUTE_UNUSED static void SignalDumpHandler(int signo)
{
    GTEST_LOG_(INFO) << "SignalDumpHandler";
    g_signalDumpFlag = true;
    EXPECT_EQ(signo, SIGDUMP) << "SignalDumpHandler Failed";
}

ATTRIBUTE_UNUSED static void SignalSegvHandler(int signo)
{
    GTEST_LOG_(INFO) << "SignalSegvHandler";
    g_signalSegvFlag = true;
    EXPECT_EQ(signo, SIGSEGV) << "SignalSegvHandler Failed";
}

ATTRIBUTE_UNUSED static void SignalDumpSigaction(int signo)
{
    GTEST_LOG_(INFO) << "SignalDumpSigaction";
    g_sigactionDumpFlag = true;
    EXPECT_EQ(signo, SIGDUMP) << "SignalDumpSigaction Failed";
}

ATTRIBUTE_UNUSED static void SignalSegvSigaction(int signo)
{
    GTEST_LOG_(INFO) << "SignalSegvSigaction";
    g_sigactionSegvFlag = true;
    EXPECT_EQ(signo, SIGSEGV) << "SignalSegvSigaction Failed";
}

ATTRIBUTE_UNUSED static void SignalIllSigaction(int signo)
{
    GTEST_LOG_(INFO) << "SignalIllSigaction";
    g_sigactionIllFlag = true;
    EXPECT_EQ(signo, SIGILL) << "SignalIllSigaction Failed";
}

ATTRIBUTE_UNUSED static bool SigchainSpecialHandlerDumpTrue(int signo, siginfo_t *si, void *ucontext)
{
    GTEST_LOG_(INFO) << "SigchainSpecialHandlerDumpTrue";
    g_sigchainDumpFlag = true;
    EXPECT_EQ(signo, SIGDUMP) << "SigchainSpecialHandlerDumpTrue Failed";
    if (g_testLastFlag) {
        g_count++;
        EXPECT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_2) << "SigchainSpecialHandlerDumpTrue: g_count.";
    }
    return true;
}

ATTRIBUTE_UNUSED static bool SigchainSpecialHandlerDump1(int signo, siginfo_t *si, void *ucontext)
{
    GTEST_LOG_(INFO) << "SigchainSpecialHandlerDump1";
    g_sigchainDump1Flag = true;
    EXPECT_EQ(signo, SIGDUMP) << "SigchainSpecialHandlerDump1 Failed";
    if (g_testLastFlag) {
        g_count++;
        EXPECT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_1) << "SigchainSpecialHandlerDump1: g_count.";
    }
    return false;
}

ATTRIBUTE_UNUSED static bool SigchainSpecialHandlerDump2(int signo, siginfo_t *si, void *ucontext)
{
    GTEST_LOG_(INFO) << "SigchainSpecialHandlerDump2";
    g_sigchainDump2Flag = true;
    EXPECT_EQ(signo, SIGDUMP) << "SigchainSpecialHandlerDump2 Failed";
    if (g_testLastFlag) {
        g_count++;
        EXPECT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_2) << "SigchainSpecialHandlerDump2: g_count.";
    }
    return false;
}

ATTRIBUTE_UNUSED static bool SigchainSpecialHandlerSegvTrue(int signo, siginfo_t *si, void *ucontext)
{
    GTEST_LOG_(INFO) << "SigchainSpecialHandlerSegvTrue";
    g_sigchainSegvFlag = true;
    EXPECT_EQ(signo, SIGSEGV) << "SigchainSpecialHandlerSegvTrue Failed";
    if (g_testLastFlag) {
        g_count++;
        EXPECT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_2) << "SigchainSpecialHandlerSegvTrue: g_count.";
    }
    return true;
}

ATTRIBUTE_UNUSED static bool SigchainSpecialHandlerSegv1(int signo, siginfo_t *si, void *ucontext)
{
    GTEST_LOG_(INFO) << "SigchainSpecialHandlerSegv1";
    g_sigchainSegv1Flag = true;
    EXPECT_EQ(signo, SIGSEGV) << "SigchainSpecialHandlerSegv1 Failed";
    if (g_testLastFlag) {
        g_count++;
        EXPECT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_1) << "SigchainSpecialHandlerSegv1: g_count.";
    }
    return false;
}

ATTRIBUTE_UNUSED static bool SigchainSpecialHandlerSegv2(int signo, siginfo_t *si, void *ucontext)
{
    GTEST_LOG_(INFO) << "SigchainSpecialHandlerSegv2";
    g_sigchainSegv2Flag = true;
    EXPECT_EQ(signo, SIGSEGV) << "SigchainSpecialHandlerSegv2 Failed";
    if (g_testLastFlag) {
        g_count++;
        EXPECT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_2) << "SigchainSpecialHandlerSegv2: g_count.";
    }
    return false;
}

class MixStackDumper {
public:
    MixStackDumper() = default;
    ~MixStackDumper() = default;
    ATTRIBUTE_UNUSED static bool DumpSignalHandler(int signo, siginfo_t *si, void *ucontext)
    {
        std::shared_ptr<int> ptr = std::make_shared<int>(TEST_PTR_VALUE);
        GTEST_LOG_(INFO) << "DumpSignalHandler: " << ptr.use_count();
        g_sigchainDump2Flag = true;
        EXPECT_EQ(signo, SIGDUMP) << "DumpSignalHandler Failed";
        return true;
    }

    ATTRIBUTE_UNUSED static bool SegvSignalHandler(int signo, siginfo_t *si, void *ucontext)
    {
        std::shared_ptr<int> ptr = std::make_shared<int>(TEST_PTR_VALUE);
        GTEST_LOG_(INFO) << "SegvSignalHandler: " << ptr.use_count();
        g_sigchainSegv2Flag = true;
        EXPECT_EQ(signo, SIGSEGV) << "SegvSignalHandler Failed";
        return false;
    }
};

static int KillAndWaitPid(int pid)
{
    usleep(SLEEP_10_MS);
    kill(pid, SIGDUMP);
    usleep(SLEEP_1000_MS);
    kill(pid, SIGSEGV);
    usleep(SLEEP_2000_MS);
    int status;
    int ret = waitpid(pid, &status, 0);
    return ret;
}

/**
 * @tc.name: SignalChainTest001
 * @tc.desc: test SignalHandler signal
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest001: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest001: pid:" << getpid();
        signal(SIGSEGV, SignalSegvHandler);
        usleep(SLEEP_1000_MS);
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_signalSegvFlag, true) << "SignalChainTest001: g_signalSegvFlag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest001: end.";
}

/**
 * @tc.name: SignalChainTest002
 * @tc.desc: test SignalHandler sigaction
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest002: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest002: pid:" << getpid();
        struct sigaction sigsegv = {
            .sa_handler = SignalSegvSigaction,
        };
        sigaction(SIGSEGV, &sigsegv, NULL);
        usleep(SLEEP_1000_MS);
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigactionSegvFlag, true) << "SignalChainTest002: g_sigactionSegvFlag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest002: end.";
}

/**
 * @tc.name: SignalChainTest003
 * @tc.desc: test SignalHandler add sigchain no else signal or sigaction
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest003: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest003: pid:" << getpid();
        struct signal_chain_action sigchain1 = {
            .sca_sigaction = SigchainSpecialHandlerDumpTrue,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain1);
        struct signal_chain_action sigsegv1 = {
            .sca_sigaction = SigchainSpecialHandlerSegvTrue,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv1);

        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainDumpFlag, true) << "SignalChainTest003: g_sigchainDumpFlag.";
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainSegvFlag, true) << "SignalChainTest003: g_sigchainSegvFlag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest003: end.";
}

/**
 * @tc.name: SignalChainTest004
 * @tc.desc: test SignalHandler add sigchain and have signal handler
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest004: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest004: pid:" << getpid();
        signal(SIGSEGV, SignalSegvHandler);

        struct signal_chain_action sigchain1 = {
            .sca_sigaction = SigchainSpecialHandlerDump1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain1);
        struct signal_chain_action sigsegv1 = {
            .sca_sigaction = SigchainSpecialHandlerSegv1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv1);

        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainDump1Flag, true) << "SignalChainTest004: g_sigchainDump1Flag.";
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_signalSegvFlag, true) << "SignalChainTest004: g_signalSegvFlag.";
        ASSERT_EQ(g_sigchainSegv1Flag, true) << "SignalChainTest004: g_sigchainSegv1Flag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest004: end.";
}

/**
 * @tc.name: SignalChainTest005
 * @tc.desc: test SignalHandler remove sigchain
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest005: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest005: pid:" << getpid();
        struct signal_chain_action sigchain1 = {
            .sca_sigaction = SigchainSpecialHandlerDump1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain1);
        struct signal_chain_action sigsegv1 = {
            .sca_sigaction = SigchainSpecialHandlerSegv1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv1);

        remove_special_signal_handler(SIGDUMP, SigchainSpecialHandlerDump1);
        remove_special_signal_handler(SIGSEGV, SigchainSpecialHandlerSegv1);

        struct signal_chain_action sigchain2 = {
            .sca_sigaction = SigchainSpecialHandlerDumpTrue,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain2);
        struct signal_chain_action sigsegv2 = {
            .sca_sigaction = SigchainSpecialHandlerSegvTrue,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv2);

        usleep(SLEEP_1000_MS);
        ASSERT_NE(g_sigchainDump1Flag, true) << "SignalChainTest005: g_sigchainDump1Flag.";
        ASSERT_EQ(g_sigchainDumpFlag, true) << "SignalChainTest005: g_sigchainDumpFlag.";
        usleep(SLEEP_1000_MS);
        ASSERT_NE(g_sigchainSegv1Flag, true) << "SignalChainTest005: g_sigchainSegv1Flag.";
        ASSERT_EQ(g_sigchainSegvFlag, true) << "SignalChainTest005: g_sigchainSegvFlag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest005: end.";
}

/**
 * @tc.name: SignalChainTest006
 * @tc.desc: test SignalHandler remove all sigchain
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest006: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest006: pid:" << getpid();
        signal(SIGDUMP, SignalDumpHandler);
        signal(SIGSEGV, SignalSegvHandler);

        struct signal_chain_action sigchain1 = {
            .sca_sigaction = SigchainSpecialHandlerDump1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain1);

        struct signal_chain_action sigsegv1 = {
            .sca_sigaction = SigchainSpecialHandlerSegv1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv1);

        remove_all_special_handler(SIGDUMP);
        remove_all_special_handler(SIGSEGV);

        usleep(SLEEP_1000_MS);
        ASSERT_NE(g_sigchainDump1Flag, true) << "SignalChainTest006: g_sigchainDump1Flag.";
        ASSERT_NE(g_sigchainDump2Flag, true) << "SignalChainTest006: g_sigchainDump2Flag.";
        ASSERT_EQ(g_signalDumpFlag, true) << "SignalChainTest006: g_signalDumpFlag.";
        usleep(SLEEP_1000_MS);
        ASSERT_NE(g_sigchainSegv1Flag, true) << "SignalChainTest006: g_sigchainSegv1Flag.";
        ASSERT_NE(g_sigchainSegv2Flag, true) << "SignalChainTest006: g_sigchainSegv2Flag.";
        ASSERT_EQ(g_signalSegvFlag, true) << "SignalChainTest006: g_signalSegvFlag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest006: end.";
}

/**
 * @tc.name: SignalChainTest007
 * @tc.desc: test SignalHandler run C++ code in sigchain handler
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest007: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest007: pid:" << getpid();
        remove_all_special_handler(SIGDUMP);
        remove_all_special_handler(SIGSEGV);

        struct signal_chain_action sigchain1 = {
            .sca_sigaction = SigchainSpecialHandlerDump1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain1);
        struct signal_chain_action sigchain2 = {
            .sca_sigaction = &(MixStackDumper::DumpSignalHandler),
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain2);

        struct signal_chain_action sigsegv1 = {
            .sca_sigaction = SigchainSpecialHandlerSegv1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv1);
        struct signal_chain_action sigsegv2 = {
            .sca_sigaction = MixStackDumper::SegvSignalHandler,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv2);

        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainDump1Flag, true) << "SignalChainTest007: g_sigchainDump1Flag.";
        ASSERT_EQ(g_sigchainDump2Flag, true) << "SignalChainTest007: g_sigchainDump2Flag.";
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainSegv1Flag, true) << "SignalChainTest007: g_sigchainSegv1Flag.";
        ASSERT_EQ(g_sigchainSegv2Flag, true) << "SignalChainTest007: g_sigchainSegv2Flag.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest007: end.";
}

/**
 * @tc.name: SignalChainTest008
 * @tc.desc: test SignalHandler add_special_handler_at_last
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest008: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest008: pid:" << getpid();
        remove_all_special_handler(SIGDUMP);
        remove_all_special_handler(SIGSEGV);

        struct signal_chain_action sigchain2 = {
            .sca_sigaction = SigchainSpecialHandlerDumpTrue,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_handler_at_last(SIGDUMP, &sigchain2);

        struct signal_chain_action sigchain1 = {
            .sca_sigaction = SigchainSpecialHandlerDump1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGDUMP, &sigchain1);

        struct signal_chain_action sigsegv2 = {
            .sca_sigaction = SigchainSpecialHandlerSegvTrue,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_handler_at_last(SIGSEGV, &sigsegv2);
        struct signal_chain_action sigsegv1 = {
            .sca_sigaction = SigchainSpecialHandlerSegv1,
            .sca_mask = {},
            .sca_flags = 0,
        };
        add_special_signal_handler(SIGSEGV, &sigsegv1);

        ResetCount();
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainDump1Flag, true) << "SignalChainTest008: g_sigchainDump1Flag.";
        ASSERT_EQ(g_sigchainDumpFlag, true) << "SignalChainTest008: g_sigchainDumpFlag.";
        ASSERT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_2) << "SignalChainTest008: dump g_count.";
        ResetCount();
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigchainSegv1Flag, true) << "SignalChainTest008: g_sigchainSegv1Flag.";
        ASSERT_EQ(g_sigchainSegvFlag, true) << "SignalChainTest008: g_sigchainSegvFlag.";
        ASSERT_EQ(g_count, SIGCHIAN_TEST_SIGNAL_NUM_2) << "SignalChainTest008: segv g_count.";
        _exit(0);
    } else {
        KillAndWaitPid(pid);
    }
    GTEST_LOG_(INFO) << "SignalChainTest008: end.";
}

/**
 * @tc.name: SignalChainTest009
 * @tc.desc: test SignalHandler special signal(SIGILL)
 * @tc.type: FUNC
 */
HWTEST_F(SignalChainTest, SignalChainTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalChainTest009: start.";
    SignalInit();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        GTEST_LOG_(INFO) << "SignalChainTest009: pid:" << getpid();
        struct sigaction sigill = {
            .sa_handler = SignalIllSigaction,
        };
        sigaction(SIGILL, &sigill, NULL);
        usleep(SLEEP_1000_MS);
        ASSERT_EQ(g_sigactionIllFlag, true) << "SignalChainTest009: g_sigactionIllFlag.";
        _exit(0);
    } else {
        usleep(SLEEP_10_MS);
        kill(pid, SIGILL);
        usleep(SLEEP_1000_MS);
        int status;
        waitpid(pid, &status, 0);
    }
    GTEST_LOG_(INFO) << "SignalChainTest009: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
