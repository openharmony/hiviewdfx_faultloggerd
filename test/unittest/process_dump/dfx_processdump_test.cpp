/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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
#include <fstream>
#include <map>
#include <csignal>
#include <dlfcn.h>
#include <string>
#include <syscall.h>
#include <unistd.h>
#include <vector>

#include "dfx_buffer_writer.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "dfx_socket_request.h"
#include "multithread_constructor.h"
#include "process_dumper.h"
#include "faultlogger_client_msg.h"

#ifndef is_ohos_lite
#include "file_util.h"
#include "hitrace/hitracechainc.h"

#define HITRACE_CHAIN_ID_MASK 0x0FFFFFFFFFFFFFFF
#endif

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

using RecordAppExitReason = int (*)(int reason, const char *exitMsg);

namespace OHOS {
namespace HiviewDFX {
class DfxProcessDumpTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

static bool CheckCppCrashKeyWords(const string& filePath, pid_t pid, int sig)
{
    if (filePath.empty() || pid <= 0) {
        return false;
    }
    map<int, string> sigKey = {
        { SIGILL, string("SIGILL") },
        { SIGTRAP, string("SIGTRAP") },
        { SIGABRT, string("SIGABRT") },
        { SIGBUS, string("SIGBUS") },
        { SIGFPE, string("SIGFPE") },
        { SIGSEGV, string("SIGSEGV") },
        { SIGSTKFLT, string("SIGSTKFLT") },
        { SIGSYS, string("SIGSYS") },
    };
    string sigKeyword = "";
    map<int, string>::iterator iter = sigKey.find(sig);
    if (iter != sigKey.end()) {
        sigKeyword = iter->second;
    }
    string keywords[] = {
        "Pid:" + to_string(pid), "Uid:", "test_processdump", "Rss", sigKeyword, "Tid:", "#00", "Registers:",
        REGISTERS, "FaultStack:", "Maps:", "test_processdump"
    };
    int length = sizeof(keywords) / sizeof(keywords[0]);
    int minRegIdx = 7; // 7 : index of REGISTERS
    int count = CheckKeyWords(filePath, keywords, length, minRegIdx);
    return count == length;
}
namespace {
bool CheckCppCrashExtraKeyWords(const string& filePath, std::string *keywords, int length, int minRegIdx)
{
    if (filePath.empty()) {
        return false;
    }
    int count = CheckKeyWords(filePath, keywords, length, minRegIdx);
    return count == length;
}
/**
 * @tc.name: DfxProcessDumpTest001
 * @tc.desc: test SIGILL crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest001: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGILL);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGILL));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest001: end.";
}

/**
 * @tc.name: DfxProcessDumpTest002
 * @tc.desc: test SIGTRAP crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest002: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGTRAP);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGTRAP));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest002: end.";
}

/**
 * @tc.name: DfxProcessDumpTest003
 * @tc.desc: test SIGABRT crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest003: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGABRT);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGABRT));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest003: end.";
}

/**
 * @tc.name: DfxProcessDumpTest004
 * @tc.desc: test SIGBUS crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest004: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGBUS);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGBUS));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest004: end.";
}

/**
 * @tc.name: DfxProcessDumpTest005
 * @tc.desc: test SIGFPE crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest005: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGFPE);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGFPE));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest005: end.";
}

/**
 * @tc.name: DfxProcessDumpTest006
 * @tc.desc: test SIGSEGV crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest006: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    GTEST_LOG_(INFO) << "process pid:" << testProcess;
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGSEGV);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGSEGV));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest006: end.";
}

/**
 * @tc.name: DfxProcessDumpTest007
 * @tc.desc: test SIGSTKFLT crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest007: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGSTKFLT);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGSTKFLT));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest007: end.";
}

/**
 * @tc.name: DfxProcessDumpTest008
 * @tc.desc: test SIGSYS crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest008: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    auto curTime = GetTimeMilliSeconds();
    kill(testProcess, SIGSYS);
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_EQ(std::to_string(curTime).length(), filename.length() - filename.find_last_of('-') - 1);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGSYS));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest008: end.";
}

/**
 * @tc.name: DfxProcessDumpTest009
 * @tc.desc: test processdump command
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest009: start.";
    string procCMD = "processdump";
    string procDumpLog = ExecuteCommands(procCMD);
    string log[] = {"please use dumpcatcher"};
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DfxProcessDumpTest009 Failed";
    GTEST_LOG_(INFO) << "DfxProcessDumpTest009: end.";
}

/**
 * @tc.name: DfxProcessDumpTest010
 * @tc.desc: test processdump command: -p 1
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest010: start.";
    string procCMD = "processdump -p 1";
    string procDumpLog = ExecuteCommands(procCMD);
    string log[] = {"please use dumpcatcher"};
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DfxProcessDumpTest010 Failed";
    GTEST_LOG_(INFO) << "DfxProcessDumpTest010: end.";
}

/**
 * @tc.name: DfxProcessDumpTest011
 * @tc.desc: Testing the sub thread crash of multithreaded programs
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest011: start.";
    pid_t testProcess = CreateMultiThreadForThreadCrash(10); // 10 : create a process with ten threads
    GTEST_LOG_(INFO) << "process pid:" << testProcess;
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    ASSERT_TRUE(CheckCppCrashKeyWords(filename, testProcess, SIGSEGV));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest011: end.";
}


/**
 * @tc.name: DfxProcessDumpTest012
 * @tc.desc: Testing new add key word
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest012: start.";
    pid_t testProcess = CreateMultiThreadForThreadCrash(10); // 10 : create a process with ten threads
    GTEST_LOG_(INFO) << "process pid:" << testProcess;
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    string keywords[] = {
        "time", "OpenFiles:"
    };
    int length = sizeof(keywords) / sizeof(keywords[0]);
    int minRegIdx = -1; // -1 : no not check register value
    ASSERT_TRUE(CheckCppCrashExtraKeyWords(filename, keywords, length, minRegIdx));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest012: end.";
}

/**
 * @tc.name: DfxProcessDumpTest013
 * @tc.desc: Testing new add key word
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest013: start.";
    int openNum = 128;
    pid_t testProcess = CreateMultiThreadForThreadCrashWithOpen(10, openNum); // 10 : create a process with ten threads
    GTEST_LOG_(INFO) << "process pid:" << testProcess;
    auto filename = WaitCreateCrashFile("cppcrash", testProcess);
    string keywords[openNum];
    string str = "FILE*";
    for (int i = 0; i < openNum; ++i) {
        keywords[i] = str;
    }
    int length = sizeof(keywords) / sizeof(keywords[0]);
    int minRegIdx = -1; // -1 : no not check register value
    ASSERT_TRUE(CheckCppCrashExtraKeyWords(filename, keywords, length, minRegIdx));
    GTEST_LOG_(INFO) << "DfxProcessDumpTest013: end.";
}

/**
 * @tc.name: DfxProcessDumpTest014
 * @tc.desc: Testing dlopen and dlsym interfaces
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest014: start.";
    void* handle = dlopen("libfaultlogger.z.so", RTLD_LAZY | RTLD_NODELETE);
    ASSERT_TRUE(handle) << "Failed to dlopen libfaultlogger";
    auto addFaultLog = reinterpret_cast<void (*)(FaultDFXLOGIInner*)>(dlsym(handle, "AddFaultLog"));
    ASSERT_TRUE(addFaultLog) << "Failed to dlsym addFaultLog";
    FaultDFXLOGIInner info;
    info.time = time(NULL);
    info.id = 0;
    info.pid = 1;
    info.pipeFd = -1;
    info.faultLogType = 2; // 2 : CPP_CRASH_TYPE
    info.logFileCutoffSizeBytes = 0;
    info.module = "";
    info.reason = "";
    info.summary = "";
    info.registers = "";
    addFaultLog(&info);
    dlclose(handle);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest014: end.";
}

/**
 * @tc.name: DfxProcessDumpTest015
 * @tc.desc: Testing dlopen and dlsym RecordAppExitReason
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest015: start.";
    void* handle = dlopen("libability_manager_c.z.so", RTLD_LAZY | RTLD_NODELETE);
    ASSERT_TRUE(handle) << "Failed to dlopen libability_manager_c";
    RecordAppExitReason recordAppExitReason = (RecordAppExitReason)dlsym(handle, "RecordAppExitReason");
    ASSERT_TRUE(recordAppExitReason) << "Failed to dlsym RecordAppExitReason";
    string reason_ = "reason";
    const int cppCrashExitReason = 2;
    recordAppExitReason(cppCrashExitReason, reason_.c_str());
    dlclose(handle);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest015: end.";
}

/**
 * @tc.name: DfxProcessDumpTest017
 * @tc.desc: Testing InitDfxProcess、InitRegs exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest017: start.";
    ProcessDumper& ins = ProcessDumper::GetInstance();
    ASSERT_FALSE(ins.InitDfxProcess());

    ins.request_.pid = -1;
    ins.request_.nsPid = -1;
    ASSERT_FALSE(ins.InitDfxProcess());
    GTEST_LOG_(INFO) << "DfxProcessDumpTest017: end.";
}

/**
 * @tc.name: DfxProcessDumpTest018
 * @tc.desc: Testing InitDfxProcess Function
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest018: start.";
    DfxBufferWriter& ins = DfxBufferWriter::GetInstance();
    ProcessDumpRequest request {};
    request.type = ProcessDumpType::DUMP_TYPE_MEM_LEAK;
    ASSERT_TRUE(ins.InitBufferWriter(request));

    request.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    ASSERT_TRUE(ins.InitBufferWriter(request));

    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    ASSERT_FALSE(ins.InitBufferWriter(request));

    auto& dumperIns = ProcessDumper::GetInstance();
    dumperIns.WriteDumpResIfNeed(DumpErrorCode::DUMP_ESUCCESS);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest018: end.";
}

#ifndef is_ohos_lite
static pid_t CreateProcessCrashWithHitraceId(uint64_t& hitraceChainId)
{
    int pipeFd[2]; // 2 : pipe fd num
    if (pipe(pipeFd) == -1) {
        GTEST_LOG_(ERROR) << "Failed to create pipe.";
        return 0;
    }
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Fail to fork new test process";
        return 0;
    } else if (pid == 0) {
        close(pipeFd[0]);
        HiTraceIdStruct hitraceId = HiTraceChainBegin("test", HITRACE_FLAG_DEFAULT);
        uint64_t val = static_cast<uint64_t>(hitraceId.chainId & HITRACE_CHAIN_ID_MASK);
        write(pipeFd[1], &val, sizeof(val));
        close(pipeFd[1]);
        raise(SIGSEGV);
    }
    close(pipeFd[1]);
    read(pipeFd[0], &hitraceChainId, sizeof(hitraceChainId));
    return pid;
}

/**
 * @tc.name: DfxProcessDumpTest019
 * @tc.desc: Testing get hitrace id
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest019: start.";
    uint64_t hitraceChainId = 0;
    pid_t testProcess = CreateProcessCrashWithHitraceId(hitraceChainId);
    ASSERT_TRUE(testProcess > 0) << "Fail to fork test process";
    GTEST_LOG_(INFO) << "Process pid: " << testProcess;
    ASSERT_NE(hitraceChainId, 0) << "Fail to set process hitrace id";
    std::regex reg(R"(cppcrash-test_processdump-0-\d{17}.log)");
    const std::string folder = "/data/log/faultlog/faultlogger/";
    auto filename = WaitCreateFile(folder, reg);
    ASSERT_FALSE(filename.empty()) << "Fail to create crash file";
    GTEST_LOG_(INFO) << "fileName: " << filename;
    std::stringstream ss;
    ss << std::hex << hitraceChainId;
    std::string hexStr = ss.str();
    GTEST_LOG_(INFO) << "hitrace id: " << hexStr;
    std::string fileContext;
    ASSERT_TRUE(LoadStringFromFile(filename, fileContext)) << "Get file context fail";
    size_t pos = fileContext.rfind(hexStr);
    ASSERT_TRUE(pos != std::string::npos) << "find no hitrace id in crash hilog";
    GTEST_LOG_(INFO) << "hitrace pos: " << pos;
    GTEST_LOG_(INFO) << "DfxProcessDumpTest019: end.";
}
#endif
}
