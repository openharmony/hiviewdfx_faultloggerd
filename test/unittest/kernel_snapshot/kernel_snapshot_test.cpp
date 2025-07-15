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

#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#include "dfx_test_util.h"
#include "kernel_snapshot_parser.h"
#include "kernel_snapshot_printer.h"
#include "kernel_snapshot_processor_impl.h"
#include "kernel_snapshot_reporter.h"
#include "kernel_snapshot_trie.h"
#include "kernel_snapshot_util.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace {
const char * const KERNEL_SNAPSHOT_EXECPTION_FILE = "/data/test/resource/testdata/kernel_snapshot_execption.txt";
const char * const KERNEL_SNAPSHOT_2_EXECPTION_FILE = "/data/test/resource/testdata/kernel_snapshot_2_execption.txt";
const char * const KERNEL_SNAPSHOT_ABORT_FILE = "/data/test/resource/testdata/kernel_snapshot_abort.txt";
const char * const STOP_FAULTLOGGERD = "service_control stop faultloggerd";
const char * const START_FAULTLOGGERD = "service_control start faultloggerd";
}

class KernelSnapshotTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void KernelSnapshotTest::SetUpTestCase()
{
    if (ExecuteCommands("uname").find("Linux") != std::string::npos) {
        return;
    }
    system(STOP_FAULTLOGGERD);
    system("param set kernel_snapshot_check_interval 3");
    system(START_FAULTLOGGERD);
}

void KernelSnapshotTest::TearDownTestCase()
{
    if (ExecuteCommands("uname").find("Linux") != std::string::npos) {
        return;
    }
    system(STOP_FAULTLOGGERD);
    system("param set kernel_snapshot_check_interval 60");
    system(START_FAULTLOGGERD);
}

void KernelSnapshotTest::SetUp()
{}

void KernelSnapshotTest::TearDown()
{}

namespace {
#if defined(__aarch64__)
void BlockSignals()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGSEGV);
    sigaddset(&set, SIGBUS);
    sigaddset(&set, SIGFPE);
    sigaddset(&set, SIGILL);
    sigaddset(&set, SIGABRT);
    sigprocmask(SIG_BLOCK, &set, nullptr);
}
#endif

std::string g_snapshotCont;

class TestKernelSnapshotProcessor : public IKernelSnapshotProcessor {
public:
    void Process(const std::string& snapshot) override
    {
        g_snapshotCont = snapshot;
        GTEST_LOG_(INFO) << "update snapshot: " << snapshot;
    }
};
} // namespace

/**
 * @tc.name: KernelSnapshotTest002
 * @tc.desc: test KernelSnapshotProcessorImpl
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest002, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest002: start.";
    std::shared_ptr<IKernelSnapshotProcessor> processor = std::make_shared<KernelSnapshotProcessorImpl>();
    std::ifstream file(KERNEL_SNAPSHOT_EXECPTION_FILE);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        processor->Process(content);
        std::string filePath = "/data/log/faultlog/temp/cppcrash-799-1736169993223";
        auto ret = access(filePath.c_str(), F_OK);
        EXPECT_EQ(ret, 0);

        std::string cmd = "rm -rf " + filePath;
        system(cmd.c_str());
    } else {
        FAIL() << "Failed to open file: " << KERNEL_SNAPSHOT_EXECPTION_FILE;
    }

    GTEST_LOG_(INFO) << "KernelSnapshotTest002: end.";
}

#if defined(__aarch64__)
/**
 * @tc.name: KernelSnapshotTest003
 * @tc.desc: test snapshot generation
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest003: start.";

    pid_t pid = fork();
    if (pid == 0) {
        BlockSignals();
        int* p = (int*)55;
        *p = 0;
    }
    waitpid(pid, 0, 0);
    std::string snapshotFilePath = WaitCreateCrashFile("cppcrash", pid, 5);

    std::ifstream snapshotFile(snapshotFilePath);
    if (snapshotFile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(snapshotFile)), std::istreambuf_iterator<char>());
        EXPECT_TRUE(content.find("Build info") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Timestamp") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Pid") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Process name:") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Reason:") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Exception registers") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("#01") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Registers:") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Memory near registers:") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("FaultStack:") != std::string::npos) << "KernelSnapshotTest003 Failed";
        EXPECT_TRUE(content.find("Elfs:") != std::string::npos) << "KernelSnapshotTest003 Failed";
    } else {
        EXPECT_TRUE(false) << "KernelSnapshotTest003 Failed";
    }

    GTEST_LOG_(INFO) << "KernelSnapshotTest003: end.";
}

/**
 * @tc.name: KernelSnapshotTest004
 * @tc.desc: test snapshot generation
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest004: start.";

    std::string removeProcessdump = "mv /system/bin/processdump /system/bin/processdump.bak";
    system(removeProcessdump.c_str());

    pid_t pid = fork();
    if (pid == 0) {
        int* p = (int*)55;
        *p = 0;
    }
    waitpid(pid, 0, 0);

    std::string snapshotFilePath = WaitCreateCrashFile("cppcrash", pid, 5);
    std::ifstream snapshotFile(snapshotFilePath);
    if (snapshotFile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(snapshotFile)), std::istreambuf_iterator<char>());
        EXPECT_TRUE(content.find("Build info") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("Timestamp") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("Pid") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("Process name:") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("Reason:") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("#01") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("Registers:") != std::string::npos) << "KernelSnapshotTest004 Failed";
        EXPECT_TRUE(content.find("Elfs:") != std::string::npos) << "KernelSnapshotTest004 Failed";
    } else {
        EXPECT_TRUE(false) << "KernelSnapshotTest004 Failed";
    }

    std::string recoverProcessdump = "mv /system/bin/processdump.bak /system/bin/processdump";
    system(recoverProcessdump.c_str());

    GTEST_LOG_(INFO) << "KernelSnapshotTest004: end.";
}
#endif

/**
 * @tc.name: KernelSnapshotTest014
 * @tc.desc: test PreProcessLine
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest014: start.";

    KernelSnapshotParser parser;
    std::string srcLine = "abcd";
    ASSERT_FALSE(parser.PreProcessLine(srcLine));

    srcLine = "\tabcdefg";
    ASSERT_FALSE(parser.PreProcessLine(srcLine));

    srcLine = "[1733329272.590140][transaction start] now mono_time is [45.006871]";
    ASSERT_TRUE(parser.PreProcessLine(srcLine));
    std::string dstLine = "[transaction start] now mono_time is [45.006871][1733329272.590140]";
    ASSERT_EQ(srcLine, dstLine);

    GTEST_LOG_(INFO) << "KernelSnapshotTest014: end.";
}

/**
 * @tc.name: KernelSnapshotTest015
 * @tc.desc: test convertThreadInfoToPairs
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest015: start.";

    KernelSnapshotParser parser;
    std::string line = "pid=1, tid=2, name=main, key=val";
    std::unordered_map<std::string, std::string> pairs = parser.ConvertThreadInfoToPairs(line);
    ASSERT_EQ(pairs["pid"], "1");
    ASSERT_EQ(pairs["tid"], "2");
    ASSERT_EQ(pairs["name"], "main");

    GTEST_LOG_(INFO) << "KernelSnapshotTest015: end.";
}

/**
 * @tc.name: KernelSnapshotTest016
 * @tc.desc: test ParseTransStart
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest016: start.";

    KernelSnapshotParser parser;
    std::string line = "abcd";
    CrashMap output;
    SnapshotCell cell {SnapshotSection::TRANSACTION_START, std::vector<std::string>{line}, 0, 0};
    parser.ParseTransStart(cell, output);
    ASSERT_EQ(output[CrashSection::TIME_STAMP], "");

    line = "[AB_00][transaction start] now mono_time is [45.006871][1733329272.590140]";
    parser.ParseTransStart({SnapshotSection::TRANSACTION_START, std::vector<std::string>{line}, 0, 0}, output);
    ASSERT_EQ(output[CrashSection::TIME_STAMP], "1733329272590");

    GTEST_LOG_(INFO) << "KernelSnapshotTest016: end.";
}

/**
 * @tc.name: KernelSnapshotTest017
 * @tc.desc: test ParseThreadInfo
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest017: start.";

    KernelSnapshotParser parser;
    std::vector<std::string> lines = {
        "Thread info:",
        "name=ei.hmsapp.music, tid=5601, state=RUNNING, sctime=40.362389062, tcb_cref=502520008108a, pid=5601, \
        ppid=656, pgid=1, uid=20020048, cpu=7, cur_rq=7"
    };
    CrashMap output;
    SnapshotCell cell {SnapshotSection::THREAD_INFO, lines, 0, 1};
    parser.ParseThreadInfo(cell, output);
    ASSERT_EQ(output[CrashSection::PID], "5601");
    ASSERT_EQ(output[CrashSection::UID], "20020048");
    ASSERT_EQ(output[CrashSection::PROCESS_NAME], "ei.hmsapp.music");

    GTEST_LOG_(INFO) << "KernelSnapshotTest017: end.";
}

/**
 * @tc.name: KernelSnapshotTest018
 * @tc.desc: test ParseStackBacktrace
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest018: start.";
    std::vector<std::string> lines = {
        "Stack backtrace (pname=/system/bin/appspawn, pid=7296):",
        " [0000000000047af8][FP: 0000007e8d68c330]<???+0x0/0x0> (/system/lib64/platformsdk/libace_napi.z.so)",
        " [000000000057c33c][FP: 0000007e8d68c3f0]<???+0x0/0x0> (/system/lib64/platformsdk/libark_jsruntime.so)"
    };
    KernelSnapshotParser parser;
    CrashMap output;

    std::string frame = "#01 pc 000000000057c33c /system/lib64/platformsdk/libark_jsruntime.so";
    parser.ParseStackBacktrace({SnapshotSection::STACK_BACKTRACE, lines, 0, 2}, output);
    EXPECT_TRUE(output[CrashSection::FAULT_THREAD_INFO].find(frame) != std::string::npos);
    GTEST_LOG_(INFO) << "KernelSnapshotTest018: end.";
}

/**
 * @tc.name: KernelSnapshotTest019
 * @tc.desc: test ParseProcessRealName
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest019: start.";
    std::vector<std::string> lines = {
        "Process statistics:",
        " name         tid  state   tcb_cref      sched_cnt cpu_cur rq_cur cls rtprio ni pri pid ppid pgid",
        " SaInit1      1012 RUNNING 5022a0008106b 7         7       6       TS  -     0   20 799 1    1",
        " audio_server 799  BLOCKED 5022a0008108a 325       4       6       TS  -     0   20 799 1    1"
    };
    KernelSnapshotParser parser;
    CrashMap output;
    output[CrashSection::PID] = "799";
    parser.ParseProcessRealName({SnapshotSection::PROCESS_STATISTICS, lines, 0, 3}, output);
    ASSERT_EQ(output[CrashSection::PROCESS_NAME], "audio_server");
    GTEST_LOG_(INFO) << "KernelSnapshotTest019: end.";
}

/**
 * @tc.name: KernelSnapshotTest020
 * @tc.desc: test ProcessSnapshotSection
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest020: start.";
    KernelSnapshotParser parser;
    CrashMap output;

    std::vector<std::string> lines = {
        "[AB_00][transaction start] now mono_time is [45.006871][1733329272.590140]"
    };

    parser.ProcessSnapshotSection({SnapshotSection::TRANSACTION_START, lines, 0, 0}, output);
    ASSERT_EQ(output[CrashSection::TIME_STAMP], "1733329272590");
    GTEST_LOG_(INFO) << "KernelSnapshotTest020: end.";
}

/**
 * @tc.name: KernelSnapshotTest021
 * @tc.desc: test ParseSnapshotUnit
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest021: start.";
    KernelSnapshotParser parser;

    std::ifstream file(KERNEL_SNAPSHOT_EXECPTION_FILE);
    std::vector<std::string> lines;
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            parser.PreProcessLine(line);
            lines.push_back(line.substr(7));
        }
        file.close();
    } else {
        FAIL() << "Failed to open file: " << KERNEL_SNAPSHOT_EXECPTION_FILE;
    }

    size_t index = 0;
    CrashMap output = parser.ParseSnapshotUnit(lines, index);
    EXPECT_EQ(output[CrashSection::TIME_STAMP], "1736169993223");
    EXPECT_EQ(output[CrashSection::PID], "799");
    EXPECT_EQ(output[CrashSection::PROCESS_NAME], "audio_server");
    GTEST_LOG_(INFO) << "KernelSnapshotTest021: end.";
}

/**
 * @tc.name: KernelSnapshotTest022
 * @tc.desc: test ParseSameSeqSnapshot
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest022: start.";
    std::ifstream file(KERNEL_SNAPSHOT_2_EXECPTION_FILE);
    KernelSnapshotParser parser;
    std::vector<std::string> lines;
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            parser.PreProcessLine(line);
            lines.push_back(line.substr(7));
        }
        file.close();
    } else {
        FAIL() << "Failed to open file: " << KERNEL_SNAPSHOT_2_EXECPTION_FILE;
    }
    std::vector<CrashMap> crashMaps;
    parser.ParseSameSeqSnapshot(lines, crashMaps);

    EXPECT_EQ(crashMaps.size(), 2);
    GTEST_LOG_(INFO) << "KernelSnapshotTest022: end.";
}

/**
 * @tc.name: KernelSnapshotTest026
 * @tc.desc: test ParseSnapshot
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest026: start.";
    KernelSnapshotParser parser;
    std::string cont = "adba";
    std::vector<std::string> lines;
    auto output = parser.ParseSnapshot(lines);
    ASSERT_EQ(output.size(), 0);

    std::ifstream file(KERNEL_SNAPSHOT_ABORT_FILE);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
    } else {
        FAIL() << "Failed to open file: " << KERNEL_SNAPSHOT_ABORT_FILE;
    }

    output = parser.ParseSnapshot(lines);
    ASSERT_EQ(output.size(), 3);
    GTEST_LOG_(INFO) << "KernelSnapshotTest026: end.";
}

/**
 * @tc.name: KernelSnapshotTest023
 * @tc.desc: test SplitByNewLine
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest023: start.";
    KernelSnapshotParser parser;
    std::vector<std::string> lines;
    std::string str = "a\nb\nc";
    parser.SplitByNewLine(str, lines);
    ASSERT_EQ(lines.size(), 3);
    GTEST_LOG_(INFO) << "KernelSnapshotTest023: end.";
}

/**
 * @tc.name: KernelSnapshotTest024
 * @tc.desc: test ProcessTransStart
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest024: start.";
    KernelSnapshotParser parser;
    std::vector<std::string> lines = {
        "[transaction start] now mono_time is [45.006871][1733329272.590140]"
    };

    CrashMap output;
    size_t index = 0;
    parser.ProcessTransStart(lines, index, SNAPSHOT_SECTION_KEYWORDS[0].key, output);
    ASSERT_EQ(output[CrashSection::TIME_STAMP], "1733329272590");
    GTEST_LOG_(INFO) << "KernelSnapshotTest024: end.";
}

/**
 * @tc.name: KernelSnapshotTest025
 * @tc.desc: test ParseDefaultAction
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest025: start.";
    KernelSnapshotParser parser;
    std::vector<std::string> lines = {
        "default"
    };
    CrashMap output;
    parser.ParseDefaultAction({SnapshotSection::BASE_ACTV_DUMPED, lines, 0, lines.size()}, output);
    ASSERT_EQ(output[CrashSection::FAULT_STACK], "");
    GTEST_LOG_(INFO) << "KernelSnapshotTest025: end.";
}

/**
 * @tc.name: KernelSnapshotTest040
 * @tc.desc: test FilterEmptySection
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest040, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest040: start.";
    std::string secHead = "secHead";
    std::string secCont = "";
    std::string end = "end";

    std::string res = KernelSnapshotUtil::FilterEmptySection(secHead, secCont, end);
    ASSERT_EQ(res, "");

    secCont = "secCont";
    res = KernelSnapshotUtil::FilterEmptySection(secHead, secCont, end);
    ASSERT_EQ(res, "secHeadsecContend");
    GTEST_LOG_(INFO) << "KernelSnapshotTest040: end.";
}

/**
 * @tc.name: KernelSnapshotTest041
 * @tc.desc: test SaveSnapshot
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest041, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest041: start.";
    KernelSnapshotPrinter printer;
    CrashMap output;

    printer.SaveSnapshot(output);
    auto ret = access("/data/log/faultlog/temp/cppcrash-1", F_OK);
    ASSERT_FALSE(ret == 0);
    GTEST_LOG_(INFO) << "KernelSnapshotTest041: end.";
}

/**
 * @tc.name: KernelSnapshotTest042
 * @tc.desc: test OutputToFile
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest042, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest042: start.";
    KernelSnapshotPrinter printer;
    CrashMap output;
    std::string filePath = "/data/log/faultlog/temp/cppcrash.txt";
    printer.OutputToFile(filePath, output);

    auto ret = access(filePath.c_str(), F_OK);
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "KernelSnapshotTest042: end.";
}

/**
 * @tc.name: KernelSnapshotTest043
 * @tc.desc: test SaveSnapshots
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest043, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest043: start.";
    KernelSnapshotPrinter printer;
    std::vector<CrashMap> outputs;
    printer.SaveSnapshots(outputs);

    for (auto &output : outputs) {
        output[CrashSection::PID] = "1";
        auto ret = access("/data/log/faultlog/temp/cppcrash-1", F_OK);
        ASSERT_EQ(ret, 0);
    }
    GTEST_LOG_(INFO) << "KernelSnapshotTest043: end.";
}

/**
 * @tc.name: KernelSnapshotTest051
 * @tc.desc: test ReportCrashnoLogEvent
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest051, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest051: start.";
    KernelSnapshotReporter reporter;

    CrashMap output;
    auto ret = reporter.ReportCrashNoLogEvent(output);
    ASSERT_FALSE(ret);

    output[CrashSection::UID] = "123";
    output[CrashSection::PID] = "156";
    output[CrashSection::TIME_STAMP] = "152451571481";
    output[CrashSection::PROCESS_NAME] = "testProcess";
    ret = reporter.ReportCrashNoLogEvent(output);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "KernelSnapshotTest051: end.";
}

/**
 * @tc.name: KernelSnapshotTest052
 * @tc.desc: test kernel snapshot trie insert
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest052, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest052: start.";
    KernelSnapshotTrie trie;

    bool ret = trie.Insert("", SnapshotSection::TRANSACTION_START);
    EXPECT_EQ(ret, false);

    ret = trie.Insert("start", SnapshotSection::TRANSACTION_START);
    EXPECT_EQ(ret, true);

    GTEST_LOG_(INFO) << "KernelSnapshotTest052: end.";
}
/**
 * @tc.name: KernelSnapshotTest053
 * @tc.desc: test kernel snapshot trie match prefix
 * @tc.type: FUNC
 */
HWTEST_F(KernelSnapshotTest, KernelSnapshotTest053, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelSnapshotTest053: start.";
    KernelSnapshotTrie trie;

    for (const auto& item : SNAPSHOT_SECTION_KEYWORDS) {
        trie.Insert(item.key, item.type);
    }

    SnapshotSection type;
    bool ret = trie.MatchPrefix(std::string(SNAPSHOT_SECTION_KEYWORDS[0].key), type);
    EXPECT_EQ(ret, true);
    EXPECT_EQ(type, SNAPSHOT_SECTION_KEYWORDS[0].type);

    trie.MatchPrefix(std::string(SNAPSHOT_SECTION_KEYWORDS[1].key) + "abc", type);
    EXPECT_EQ(ret, true);
    EXPECT_EQ(type, SNAPSHOT_SECTION_KEYWORDS[1].type);

    ret = trie.MatchPrefix(std::string("abc"), type);
    EXPECT_EQ(ret, false);

    GTEST_LOG_(INFO) << "KernelSnapshotTest053: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
