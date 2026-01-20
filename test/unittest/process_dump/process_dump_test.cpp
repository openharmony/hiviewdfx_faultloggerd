/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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
#include <memory>
#include <string>

#include "dfx_elf_parser.h"
#include "dfx_ptrace.h"
#include "dfx_regs.h"
#include "dfx_regs_get.h"
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_thread.h"
#include "dump_utils.h"
#include <pthread.h>
#include "process_dumper.h"
#include "dfx_util.h"
#include "dfx_test_util.h"
#include "dfx_socket_request.h"
#include "lite_process_dumper.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
int g_pipeFd[] = {-1, -1};
class ProcessDumpTest : public testing::Test {
public:
    void SetUp() override
    {
        pipe(g_pipeFd);
        int newSize = 1024 * 1024; // 1MB
        fcntl(g_pipeFd[PIPE_WRITE], F_SETPIPE_SZ, newSize);
    }
    void TearDown() override
    {
        ClosePipeFd(g_pipeFd[PIPE_WRITE]);
        ClosePipeFd(g_pipeFd[PIPE_READ]);
    }
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
void *SleepThread(void *argv)
{
    int threadID = *(int*)argv;
    printf("create MultiThread %d", threadID);

    const int sleepTime = 10;
    sleep(sleepTime);

    return nullptr;
}

/**
 * @tc.name: DfxProcessTest003
 * @tc.desc: test init other threads
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest003, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DfxProcessTest003: start.";
    DfxProcess process;
    process.InitProcessInfo(getpid(), getpid(), getuid(), "");
    pthread_t childThread;
    int threadID[1] = {1};
    pthread_create(&childThread, NULL, SleepThread, &threadID[0]);
    pthread_detach(childThread);
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = gettid(),
        .pid = getpid(),
        .nsPid = getpid(),
    };
    auto ret = process.InitOtherThreads(request);
    EXPECT_EQ(true, ret) << "DfxProcessTest003 Failed";
    auto threads = process.GetOtherThreads();
    EXPECT_GT(threads.size(), 0) << "DfxProcessTest003 Failed";
    process.ClearOtherThreads();
    threads = process.GetOtherThreads();
    EXPECT_EQ(threads.size(), 0) << "DfxProcessTest003 Failed";
    GTEST_LOG_(INFO) << "DfxProcessTest003: end.";
}

/**
 * @tc.name: DfxProcessTest004
 * @tc.desc: test Attach Detach
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest004: start.";
    DfxProcess process;
    process.InitProcessInfo(getpid(), getpid(), getuid(), "");
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = gettid(),
        .pid = getpid(),
        .nsPid = getpid(),
    };
    auto ret = process.InitOtherThreads(request);
    EXPECT_EQ(true, ret) << "DfxProcessTest004 Failed";
    process.Attach();
    process.Detach();
    GTEST_LOG_(INFO) << "DfxProcessTest004: end.";
}

/**
 * @tc.name: DfxProcessTest005
 * @tc.desc: test DfxProcess ChangeTid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest005: start.";
    pid_t pid = getpid();
    DfxProcess process1;
    process1.InitProcessInfo(pid, pid, getuid(), "");
    pid_t ret = process1.ChangeTid(pid, false);
    process1.Attach();
    process1.Detach();
    ASSERT_EQ(ret, pid);
    pthread_t tid;
    int threadID[1] = {1};
    pthread_create(&tid, NULL, SleepThread, &threadID[0]);
    pthread_detach(tid);
    DfxProcess process2;
    process2.InitProcessInfo(pid, pid, getuid(), "");
    ret = process2.ChangeTid(pid, false);
    process2.Attach();
    process2.Detach();
    ASSERT_EQ(ret, pid);
    GTEST_LOG_(INFO) << "DfxProcessTest005: end.";
}

/**
 * @tc.name: DfxProcessTest006
 * @tc.desc: test DfxProcess ChangeTid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest006: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "");
    pid_t id = process.ChangeTid(pid, false);
    process.GetKeyThread() = DfxThread::Create(pid, pid, pid);
    process.Attach(true);
    DfxPtrace::Detach(pid);
    ASSERT_TRUE(id != 0);
    GTEST_LOG_(INFO) << "DfxProcessTest006: end.";
}

/**
 * @tc.name: DfxProcessTest007
 * @tc.desc: test DfxProcess ChangeTid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest007: start.";
    pid_t curPid = getpid();
    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, WNOHANG);
    DfxProcess process;
    process.InitProcessInfo(curPid, pid, getuid(), "");
    pid_t id = process.ChangeTid(pid, false);
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = pid,
        .pid = pid,
        .nsPid = curPid,
    };
    process.InitKeyThread(request);
    id = process.ChangeTid(pid, false);
    ASSERT_NE(id, 0);
    GTEST_LOG_(INFO) << "DfxProcessTest007: end.";
}

/**
 * @tc.name: DfxThreadTest002
 * @tc.desc: test DfxThread GetThreadRegs
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxThreadTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThreadTest002: start.";
    int32_t pid = 243, tid = 243;
    DfxThread thread(pid, tid, tid);
    std::shared_ptr<DfxRegs> inputrefs;
    thread.SetThreadRegs(inputrefs);
    std::shared_ptr<DfxRegs> outputrefs = thread.GetThreadRegs();
    EXPECT_EQ(true, inputrefs == outputrefs) << "DfxThreadTest002 Failed";
    GTEST_LOG_(INFO) << "DfxThreadTest002: end.";
}

/**
 * @tc.name: DfxThreadTest003
 * @tc.desc: test DfxThread GetProcessInfo
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxThreadTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThreadTest003: start.";
    const int32_t tid = 99999;
    DfxThread thread(tid, tid, tid, true);
    EXPECT_EQ(true, thread.GetProcessInfo() == nullptr) << "DfxThreadTest003 Failed";
    DumpUtils::IsSelinuxPermissive();
    GTEST_LOG_(INFO) << "DfxThreadTest003: end.";
}

#if defined(__aarch64__)
/**
 * @tc.name: LiteProcessDumpTest001
 * @tc.desc: test DfxThread GetThreadRegs
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest001: start.";
    LiteProcessDumper liteDumper;
    EXPECT_FALSE(liteDumper.Dump(666666));
    GTEST_LOG_(INFO) << "LiteProcessDumpTest001: end.";
}

/**
 * @tc.name: LiteProcessDumpTest002
 * @tc.desc: test ReadRequest
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest002: start.";
    LiteProcessDumper liteDumper;
    ProcessDumpRequest request;
    request.pid = 9999;
    write(g_pipeFd[PIPE_WRITE], &request, sizeof(request));
    liteDumper.ReadRequest(g_pipeFd[PIPE_READ]);
    EXPECT_EQ(liteDumper.request_.pid, 9999);
    GTEST_LOG_(INFO) << "LiteProcessDumpTest002: end.";
}

/**
 * @tc.name: LiteProcessDumpTest003
 * @tc.desc: test LiteDump ReadStat
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest003: start.";
    LiteProcessDumper liteDumper;
    char stat[PROC_STAT_BUF_SIZE] = "3197 (ohos.sceneboard) S 631 1 1 0 0 0 287275 287275 286258 286258 30159 0 0 0"
                        " -14 -10 30 0 "
                        "1278 39994892288 147148 0 0 0 545876298272 0 0 0 0 0 0 0 9 0 0 1 13 1 0 0 0 368254310288 "
                        "368254317648 383078469632 545876298954 545876299229 545876299229 545876299728 0 0";
    EXPECT_TRUE(g_pipeFd[PIPE_WRITE] > 0 && g_pipeFd[PIPE_READ] > 0);
    EXPECT_EQ(write(g_pipeFd[PIPE_WRITE], stat, sizeof(stat)), sizeof(stat));
    liteDumper.ReadStat(g_pipeFd[PIPE_READ]);
    EXPECT_EQ(liteDumper.stat_, std::string(stat));
    GTEST_LOG_(INFO) << "LiteProcessDumpTest003: end.";
}

/**
 * @tc.name: LiteProcessDumpTest004
 * @tc.desc: test LiteDump ReadStatm
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest004: start.";
    LiteProcessDumper liteDumper;
    char statm[PROC_STATM_BUF_SIZE] = "9765969 146791 69282 17 0 141771 0";
    write(g_pipeFd[PIPE_WRITE], statm, sizeof(statm));
    liteDumper.ReadStatm(g_pipeFd[PIPE_READ]);
    EXPECT_EQ(liteDumper.statm_, std::string(statm));
    GTEST_LOG_(INFO) << "LiteProcessDumpTest004: end.";
}

/**
 * @tc.name: LiteProcessDumpTest005
 * @tc.desc: test liteDump ReadStack
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest005: start.";
    LiteProcessDumper liteDumper;
    std::string str = "test stack buf";
    write(g_pipeFd[PIPE_WRITE], str.c_str(), str.length() + 1);
    liteDumper.ReadStack(g_pipeFd[PIPE_READ]);
    std::string s(liteDumper.stackBuf_.begin(), liteDumper.stackBuf_.end());
    EXPECT_EQ(s.substr(0, str.length()), str);

    std::string sk(PRIV_COPY_STACK_BUFFER_SIZE, 'a');
    write(g_pipeFd[PIPE_WRITE], sk.c_str(), sk.length());
    liteDumper.ReadStack(g_pipeFd[PIPE_READ]);

    GTEST_LOG_(INFO) << "LiteProcessDumpTest005: end.";
}

/**
 * @tc.name: LiteProcessDumpTest006
 * @tc.desc: test LiteDump ReadMemory
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest006: start.";
    LiteProcessDumper liteDumper;
    constexpr int totalSize = 32 * 31 * 8 + 64 * 2 * 8;
    char tmpStr[totalSize] = "test memory buf";
    write(g_pipeFd[PIPE_WRITE], tmpStr, 50);
    EXPECT_EQ(write(g_pipeFd[PIPE_WRITE], tmpStr, totalSize), totalSize);
    write(g_pipeFd[PIPE_WRITE], tmpStr, 50);
    ClosePipeFd(g_pipeFd[PIPE_WRITE]);
    ProcessDumpRequest request {};
    liteDumper.ReadMemoryNearRegister(g_pipeFd[PIPE_READ], request);
    auto& ins = MemoryNearRegisterUtil::GetInstance();
    auto info = ins.blocksInfo_.front().content;

    std::string s(info.begin(), info.end());
    GTEST_LOG_(INFO) << "LiteProcessDumpTest006: info" << s;

    liteDumper.Unwind();
    liteDumper.InitProcess();
    liteDumper.PrintAll();
    GTEST_LOG_(INFO) << "LiteProcessDumpTest006: end.";
}

/**
 * @tc.name: LiteProcessDumpTest009
 * @tc.desc: test DfxBufferWriter GetFaultloggerdRequestType
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, LiteProcessDumpTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LiteProcessDumpTest009: start.";
    auto& writer = DfxBufferWriter::GetInstance();
    writer.request_ = {};
    writer.request_.siginfo.si_signo = SIGLEAK_STACK;
    writer.request_.siginfo.si_code = -SIGLEAK_STACK_FDSAN;
    EXPECT_EQ(writer.GetFaultloggerdRequestType(), FaultLoggerType::CPP_STACKTRACE);
    writer.request_.siginfo.si_code = -SIGLEAK_STACK_JEMALLOC;
    EXPECT_EQ(writer.GetFaultloggerdRequestType(), FaultLoggerType::CPP_STACKTRACE);
    writer.request_.siginfo.si_code = -SIGLEAK_STACK_BADFD;
    EXPECT_EQ(writer.GetFaultloggerdRequestType(), FaultLoggerType::CPP_STACKTRACE);
    writer.request_.siginfo.si_code = -99;
    EXPECT_EQ(writer.GetFaultloggerdRequestType(), FaultLoggerType::LEAK_STACKTRACE);

    writer.request_.siginfo.si_signo = SIGDUMP;
    EXPECT_EQ(writer.GetFaultloggerdRequestType(), FaultLoggerType::CPP_STACKTRACE);
    writer.request_.siginfo.si_signo = SIGILL;
    EXPECT_EQ(writer.GetFaultloggerdRequestType(), FaultLoggerType::CPP_CRASH);
    GTEST_LOG_(INFO) << "LiteProcessDumpTest009: end.";
}
#endif
}
