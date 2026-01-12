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
#include <unistd.h>
#include "csignal"
#include "proc_util.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
/**
 * @tc.name: ParseStatTest001
 * @tc.desc: test path invalid.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest001, TestSize.Level2)
{
    ProcessInfo info;
    ASSERT_FALSE(ParseStat("", info));
}

/**
 * @tc.name: ParseStatTest002
 * @tc.desc: parse file.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest002, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest002/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, 1716);
    ASSERT_EQ(info.comm, std::string("at-spi-bus-laun"));
    ASSERT_EQ(info.state, ThreadState::RUNNING);
    ASSERT_EQ(info.ppid, 1695);
    ASSERT_EQ(info.utime, 0);
    ASSERT_EQ(info.stime, 0);
    ASSERT_EQ(info.cutime, 0);
    ASSERT_EQ(info.cstime, 0);
    ASSERT_EQ(info.priority, 20);
    ASSERT_EQ(info.nice, 0);
    ASSERT_EQ(info.numThreads, 4);
    ASSERT_EQ(info.starttime, 1326);
    ASSERT_EQ(info.rss, 1312);
    ASSERT_EQ(info.signal, 0);
    ASSERT_EQ(info.blocked, 0);
    ASSERT_EQ(info.sigignore, 4096);
    ASSERT_EQ(info.sigcatch, 81920);
}

/**
 * @tc.name: ParseStatTest003
 * @tc.desc: parse file.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest003, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest003/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, 111);
    ASSERT_EQ(info.comm, std::string("222"));
    ASSERT_EQ(info.state, ThreadState::SLEEP);
    ASSERT_EQ(info.ppid, 444);
    ASSERT_EQ(info.utime, 1414);
    ASSERT_EQ(info.stime, 1515);
    ASSERT_EQ(info.cutime, 1616);
    ASSERT_EQ(info.cstime, 1717);
    ASSERT_EQ(info.priority, 1818);
    ASSERT_EQ(info.nice, 1919);
    ASSERT_EQ(info.numThreads, 2020);
    ASSERT_EQ(info.starttime, 2222);
    ASSERT_EQ(info.rss, 2424);
    ASSERT_EQ(info.signal, 3131);
    ASSERT_EQ(info.blocked, 3232);
    ASSERT_EQ(info.sigignore, 3333);
    ASSERT_EQ(info.sigcatch, 3434);
}

/**
 * @tc.name: ParseStatTest004
 * @tc.desc: parse file.param < 0
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest004, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest004/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, -111);
    ASSERT_EQ(info.comm, std::string("222"));
    ASSERT_EQ(info.state, ThreadState::DISK_SLEEP);
    ASSERT_EQ(info.ppid, -444);
    ASSERT_EQ(info.utime, 1414);
    ASSERT_EQ(info.stime, 1515);
    ASSERT_EQ(info.cutime, -1616);
    ASSERT_EQ(info.cstime, -1717);
    ASSERT_EQ(info.priority, -1818);
    ASSERT_EQ(info.nice, -1919);
    ASSERT_EQ(info.numThreads, -2020);
    ASSERT_EQ(info.starttime, 2222);
    ASSERT_EQ(info.rss, -2424);
    ASSERT_EQ(info.signal, 3131);
    ASSERT_EQ(info.blocked, 3232);
    ASSERT_EQ(info.sigignore, 3333);
    ASSERT_EQ(info.sigcatch, 3434);
}

/**
 * @tc.name: ParseStatTest005
 * @tc.desc: parse file. comm len > 16
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest005, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest005/stat";
    ASSERT_FALSE(ParseStat(path, info));
}

/**
 * @tc.name: ParseStatTest006
 * @tc.desc: parse file. comm contains ')'
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest006, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest006/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, -111);
    ASSERT_EQ(info.comm, std::string("222)"));
    ASSERT_EQ(info.state, ThreadState::ZOMBIE);
    ASSERT_EQ(info.ppid, -444);
    ASSERT_EQ(info.utime, 1414);
    ASSERT_EQ(info.stime, 1515);
    ASSERT_EQ(info.cutime, -1616);
    ASSERT_EQ(info.cstime, -1717);
    ASSERT_EQ(info.priority, -1818);
    ASSERT_EQ(info.nice, -1919);
    ASSERT_EQ(info.numThreads, -2020);
    ASSERT_EQ(info.starttime, 2222);
    ASSERT_EQ(info.rss, -2424);
    ASSERT_EQ(info.signal, 3131);
    ASSERT_EQ(info.blocked, 3232);
    ASSERT_EQ(info.sigignore, 3333);
    ASSERT_EQ(info.sigcatch, 3434);
}

/**
 * @tc.name: ParseStatTest007
 * @tc.desc: parse file. comm contains ') '
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest007, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest007/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, -111);
    ASSERT_EQ(info.comm, std::string("222) "));
    ASSERT_EQ(info.state, ThreadState::IDLE);
    ASSERT_EQ(info.ppid, -444);
    ASSERT_EQ(info.utime, 1414);
    ASSERT_EQ(info.stime, 1515);
    ASSERT_EQ(info.cutime, -1616);
    ASSERT_EQ(info.cstime, -1717);
    ASSERT_EQ(info.priority, -1818);
    ASSERT_EQ(info.nice, -1919);
    ASSERT_EQ(info.numThreads, -2020);
    ASSERT_EQ(info.starttime, 2222);
    ASSERT_EQ(info.rss, -2424);
    ASSERT_EQ(info.signal, 3131);
    ASSERT_EQ(info.blocked, 3232);
    ASSERT_EQ(info.sigignore, 3333);
    ASSERT_EQ(info.sigcatch, 3434);
}

/**
 * @tc.name: ParseStatTest008
 * @tc.desc: parse file. comm len = 16
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest008, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/data/test/resource/proc/ParseStatTest008/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, -111);
    ASSERT_EQ(info.comm, std::string("0123456789abcdef"));
    ASSERT_EQ(info.state, ThreadState::STOPPED);
    ASSERT_EQ(info.ppid, -444);
    ASSERT_EQ(info.utime, 1414);
    ASSERT_EQ(info.stime, 1515);
    ASSERT_EQ(info.cutime, -1616);
    ASSERT_EQ(info.cstime, -1717);
    ASSERT_EQ(info.priority, -1818);
    ASSERT_EQ(info.nice, -1919);
    ASSERT_EQ(info.numThreads, -2020);
    ASSERT_EQ(info.starttime, 2222);
    ASSERT_EQ(info.rss, -2424);
    ASSERT_EQ(info.signal, 3131);
    ASSERT_EQ(info.blocked, 3232);
    ASSERT_EQ(info.sigignore, 3333);
    ASSERT_EQ(info.sigcatch, 3434);
}

/**
 * @tc.name: ParseStatTest009
 * @tc.desc: parse file. read /proc/self/stat
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParseStatTest009, TestSize.Level2)
{
    ProcessInfo info;
    const std::string path = "/proc/self/stat";
    ASSERT_TRUE(ParseStat(path, info));
    ASSERT_EQ(info.pid, getpid());
    ASSERT_EQ(info.comm, std::string("test_common"));
}

/**
 * @tc.name: GetFirstNumberSeqTest010
 * @tc.desc: get first number sequence.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, GetFirstNumberSeqTest010, TestSize.Level2)
{
    std::string str = "abc";
    std::string result = GetFirstNumberSeq(str);
    ASSERT_EQ(result, "");

    std::string data = "123456";
    result = GetFirstNumberSeq(data);
    ASSERT_EQ(result, data);

    result = GetFirstNumberSeq("abc123456");
    ASSERT_EQ(result, data);

    result = GetFirstNumberSeq("abc123456   ");
    ASSERT_EQ(result, data);

    result = GetFirstNumberSeq("abc123456 adc");
    ASSERT_EQ(result, data);

    result = GetFirstNumberSeq("abc123456  123");
    ASSERT_EQ(result, data);

    result = GetFirstNumberSeq("123456  123");
    ASSERT_EQ(result, data);
}

/**
 * @tc.name: IsSigDumpMaskTest011
 * @tc.desc: is sig dump mask.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, IsSigDumpMaskTest011, TestSize.Level2)
{
    uint64_t sigBlk = 0x00000000;
    ASSERT_FALSE(IsSigDumpMask(sigBlk));

    sigBlk = 0x00000004;
    ASSERT_FALSE(IsSigDumpMask(sigBlk));
    // SigBlk: 0000000400000000
    sigBlk = 0x0000000400000000;
    ASSERT_TRUE(IsSigDumpMask(sigBlk));

    sigBlk = 0x4000000000;
    ASSERT_FALSE(IsSigDumpMask(sigBlk));
}

/**
 * @tc.name: ParsePidStatus013
 * @tc.desc: is hap.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, ParsePidStatus013, TestSize.Level2)
{
    pid_t pid = 99999; // 99999 : Invalid PID
    long uid = 0;
    uint64_t sigBlk = 0;
    auto result = GetUidAndSigBlk(pid, uid, sigBlk);
    EXPECT_TRUE(uid == 0);
    EXPECT_TRUE(sigBlk == 0);

    pid = getpid();
    result = GetUidAndSigBlk(pid, uid, sigBlk);
    EXPECT_TRUE(sigBlk == 0);

    sigset_t sigSet;
    sigemptyset(&sigSet);
    sigaddset(&sigSet, SIGQUIT);
    sigprocmask(SIG_BLOCK, &sigSet, nullptr);

    result = GetUidAndSigBlk(pid, uid, sigBlk);
    EXPECT_TRUE(sigBlk ==  (1 << (SIGQUIT -1)));

    sigprocmask(SIG_UNBLOCK, &sigSet, nullptr);
}

/**
 * @tc.name: GetClkTckTest001
 * @tc.desc: check GetClkTck.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, GetClkTckTest001, TestSize.Level2)
{
    ASSERT_GT(GetClkTck(), 0);
}

/**
 * @tc.name: FomatProcessInfoToStringTest001
 * @tc.desc: check FomatProcessInfoToString.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, FomatProcessInfoToStringTest001, TestSize.Level2)
{
    ProcessInfo info = {
        .state = ThreadState::RUNNING,
        .utime = 12345,
        .stime = 1234,
        .priority = -1,
        .nice = 20,
    };
    auto result = FomatProcessInfoToString(info);
    ASSERT_FALSE(result.empty());
    std::string expect("state=R, utime=12345, stime=1234, priority=-1, nice=20, clk=");
    ASSERT_EQ(result.compare(0, expect.length(), expect), 0);

    info.state = ThreadState::SLEEP;
    info.priority = 10;
    info.nice = -10;
    result = FomatProcessInfoToString(info);
    ASSERT_FALSE(result.empty());
    expect = "state=S, utime=12345, stime=1234, priority=10, nice=-10, clk=";
    ASSERT_EQ(result.compare(0, expect.length(), expect), 0);
}

/**
 * @tc.name: GetPssMemoryTest001
 * @tc.desc: check GetPssMemory.
 * @tc.type: FUNC
 */
HWTEST(ProcUtilTest, GetPssMemoryTest001, TestSize.Level2)
{
    uint32_t pssMemory = GetPssMemory();
    ASSERT_TRUE(pssMemory > 0);
}
} // namespace HiviewDFX
} // namespace OHOS
