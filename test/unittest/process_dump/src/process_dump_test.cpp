/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains process dump module unittest. */

#include "process_dump_test.h"

#include <memory>
#include <string>

#include "dfx_regs.h"
#include "dfx_dump_request.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "process_dumper.h"
#include "dfx_unwind_remote.h"
#include "dfx_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

static const int CMD_BUF_LEN = 100;

void ProcessDumpTest::SetUpTestCase(void)
{
}

void ProcessDumpTest::TearDownTestCase(void)
{
}

void ProcessDumpTest::SetUp(void)
{
}

void ProcessDumpTest::TearDown(void)
{
}

int ProcessDumpTest::GetProcessPid(std::string processName)
{
    std::string procCMD = "pgrep '" + processName + "'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    std::string processPid;
    char resultBuf[CMD_BUF_LEN] = { 0, };
    while (fgets(resultBuf, sizeof(resultBuf), procFileInfo) != nullptr) {
        processPid = resultBuf;
        GTEST_LOG_(INFO) << "applyPid: " << processPid;
    }
    pclose(procFileInfo);
    return std::atoi(processPid.c_str());
}

namespace {
/**
 * @tc.name: ProcessDumpTest001
 * @tc.desc: test get dump type
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest001: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputType = 1;
    processDump->SetType(static_cast<ProcessDumpType>(inputType));
    ProcessDumpType outputType = processDump->GetType();
    EXPECT_EQ(true, inputType == outputType) << "ProcessDumpTest001 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest001: end.";
}

/**
 * @tc.name: ProcessDumpTest002
 * @tc.desc: test get dump type
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest002: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputType = 164;
    processDump->SetType(static_cast<ProcessDumpType>(inputType));
    ProcessDumpType outputType = processDump->GetType();
    EXPECT_EQ(true, inputType == outputType) << "ProcessDumpTest002 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest002: end.";
}

/**
 * @tc.name: ProcessDumpTest003
 * @tc.desc: test get dump type
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest003: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputType = 3;
    processDump->SetType(static_cast<ProcessDumpType>(inputType));
    ProcessDumpType outputType = processDump->GetType();
    EXPECT_EQ(true, inputType == outputType) << "ProcessDumpTest003 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest003: end.";
}

/**
 * @tc.name: ProcessDumpTest004
 * @tc.desc: test get dump tid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest004: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputTid = 1;
    processDump->SetTid(inputTid);
    int32_t outputTid = processDump->GetTid();
    EXPECT_EQ(true, inputTid == outputTid) << "ProcessDumpTest004 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest004: end.";
}

/**
 * @tc.name: ProcessDumpTest005
 * @tc.desc: test get dump tid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest005: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputTid = 164;
    processDump->SetTid(inputTid);
    int32_t outputTid = processDump->GetTid();
    EXPECT_EQ(true, inputTid == outputTid) << "ProcessDumpTest005 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest005: end.";
}

/**
 * @tc.name: ProcessDumpTest006
 * @tc.desc: test get dump tid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest006: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputTid = 3;
    processDump->SetTid(inputTid);
    int32_t outputTid = processDump->GetTid();
    EXPECT_EQ(true, inputTid == outputTid) << "ProcessDumpTest006 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest006: end.";
}

/**
 * @tc.name: ProcessDumpTest007
 * @tc.desc: test get dump pid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest007: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputPid = 1;
    processDump->SetPid(inputPid);
    int32_t outputPid = processDump->GetPid();
    EXPECT_EQ(true, inputPid == outputPid) << "ProcessDumpTest007 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest007: end.";
}

/**
 * @tc.name: ProcessDumpTest008
 * @tc.desc: test get dump pid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest008: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputPid = 164;
    processDump->SetPid(inputPid);
    int32_t outputPid = processDump->GetPid();
    EXPECT_EQ(true, inputPid == outputPid) << "ProcessDumpTest008 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest008: end.";
}

/**
 * @tc.name: ProcessDumpTest009
 * @tc.desc: test get dump pid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest009: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputPid = 3;
    processDump->SetPid(inputPid);
    int32_t outputPid = processDump->GetPid();
    EXPECT_EQ(true, inputPid == outputPid) << "ProcessDumpTest009 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest009: end.";
}

/**
 * @tc.name: ProcessDumpTest010
 * @tc.desc: test get dump uid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest010: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint32_t inputUid = 1;
    processDump->SetUid(inputUid);
    uint32_t outputUid = processDump->GetUid();
    EXPECT_EQ(true, inputUid == outputUid) << "ProcessDumpTest010 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest010: end.";
}

/**
 * @tc.name: ProcessDumpTest011
 * @tc.desc: test get dump uid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest011: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint32_t inputUid = 164;
    processDump->SetUid(inputUid);
    uint32_t outputUid = processDump->GetUid();
    EXPECT_EQ(true, inputUid == outputUid) << "ProcessDumpTest011 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest011: end.";
}

/**
 * @tc.name: ProcessDumpTest012
 * @tc.desc: test get dump uid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest012: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint32_t inputUid = 3;
    processDump->SetUid(inputUid);
    uint32_t outputUid = processDump->GetUid();
    EXPECT_EQ(true, inputUid == outputUid) << "ProcessDumpTest012 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest012: end.";
}

/**
 * @tc.name: ProcessDumpTest013
 * @tc.desc: test get dump reserved
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest013: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputReserved = 1;
    processDump->SetReserved(inputReserved);
    int32_t outputReserved = processDump->GetReserved();
    EXPECT_EQ(true, inputReserved == outputReserved) << "ProcessDumpTest013 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest013: end.";
}

/**
 * @tc.name: ProcessDumpTest014
 * @tc.desc: test get dump reserved
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest014: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputReserved = 164;
    processDump->SetReserved(inputReserved);
    int32_t outputReserved = processDump->GetReserved();
    EXPECT_EQ(true, inputReserved == outputReserved) << "ProcessDumpTest014 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest014: end.";
}

/**
 * @tc.name: ProcessDumpTest015
 * @tc.desc: test get dump reserved
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest015: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    int32_t inputReserved = 3;
    processDump->SetReserved(inputReserved);
    int32_t outputReserved = processDump->GetReserved();
    EXPECT_EQ(true, inputReserved == outputReserved) << "ProcessDumpTest015 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest015: end.";
}

/**
 * @tc.name: ProcessDumpTest016
 * @tc.desc: test get dump timeStamp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest016: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint64_t inputTimeStamp= 1;
    processDump->SetTimeStamp(inputTimeStamp);
    uint64_t outputTimeStamp = processDump->GetTimeStamp();
    EXPECT_EQ(true, inputTimeStamp == outputTimeStamp) << "ProcessDumpTest016 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest016: end.";
}

/**
 * @tc.name: ProcessDumpTest017
 * @tc.desc: test get dump timeStamp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest017: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint64_t inputTimeStamp = 164;
    processDump->SetTimeStamp(inputTimeStamp);
    uint64_t outputTimeStamp = processDump->GetTimeStamp();
    EXPECT_EQ(true, inputTimeStamp == outputTimeStamp) << "ProcessDumpTest017 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest017: end.";
}

/**
 * @tc.name: ProcessDumpTest018
 * @tc.desc: test get dump timeStamp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest018: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint64_t inputTimeStamp= 3;
    processDump->SetTimeStamp(inputTimeStamp);
    uint64_t outputTimeStamp = processDump->GetTimeStamp();
    EXPECT_EQ(true, inputTimeStamp == outputTimeStamp) << "ProcessDumpTest018 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest018: end.";
}

/**
 * @tc.name: ProcessDumpTest019
 * @tc.desc: test get dump timeStamp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest019: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint64_t inputTimeStamp= 4;
    processDump->SetTimeStamp(inputTimeStamp);
    uint64_t outputTimeStamp = processDump->GetTimeStamp();
    EXPECT_EQ(true, inputTimeStamp == outputTimeStamp) << "ProcessDumpTest019 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest019: end.";
}

/**
 * @tc.name: ProcessDumpTest020
 * @tc.desc: test get dump timeStamp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest020: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    uint64_t inputTimeStamp= 5;
    processDump->SetTimeStamp(inputTimeStamp);
    uint64_t outputTimeStamp = processDump->GetTimeStamp();
    EXPECT_EQ(true, inputTimeStamp == outputTimeStamp) << "ProcessDumpTest020 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest020: end.";
}

/**
 * @tc.name: ProcessDumpTest021
 * @tc.desc: test get dump sigInfo
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest021: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    siginfo_t inputSigInfo;
    inputSigInfo.si_pid = 1;
    processDump->SetSiginfo(inputSigInfo);
    siginfo_t outputSigInfo = processDump->GetSiginfo();
    EXPECT_EQ(true, outputSigInfo.si_pid == inputSigInfo.si_pid) << "ProcessDumpTest021 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest021: end.";
}

/**
 * @tc.name: ProcessDumpTest022
 * @tc.desc: test get dump sigInfo
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest022: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    siginfo_t inputSigInfo;
    inputSigInfo.si_pid = 1;
    processDump->SetSiginfo(inputSigInfo);
    siginfo_t outputSigInfo = processDump->GetSiginfo();
    EXPECT_EQ(true, outputSigInfo.si_pid == inputSigInfo.si_pid) << "ProcessDumpTest022 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest022: end.";
}

/**
 * @tc.name: ProcessDumpTest023
 * @tc.desc: test get dump sigInfo
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest023: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    siginfo_t inputSigInfo;
    inputSigInfo.si_pid = 1;
    processDump->SetSiginfo(inputSigInfo);
    siginfo_t outputSigInfo = processDump->GetSiginfo();
    EXPECT_EQ(true, outputSigInfo.si_pid == inputSigInfo.si_pid) << "ProcessDumpTest023 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest023: end.";
}

/**
 * @tc.name: ProcessDumpTest024
 * @tc.desc: test get dump sigInfo
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest024: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    siginfo_t inputSigInfo;
    inputSigInfo.si_pid = 1;
    processDump->SetSiginfo(inputSigInfo);
    siginfo_t outputSigInfo = processDump->GetSiginfo();
    EXPECT_EQ(true, outputSigInfo.si_pid == inputSigInfo.si_pid) << "ProcessDumpTest024 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest024: end.";
}

/**
 * @tc.name: ProcessDumpTest025
 * @tc.desc: test get dump sigInfo
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest025: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    siginfo_t inputSigInfo;
    inputSigInfo.si_pid = 1;
    processDump->SetSiginfo(inputSigInfo);
    siginfo_t outputSigInfo = processDump->GetSiginfo();
    EXPECT_EQ(true, outputSigInfo.si_pid == inputSigInfo.si_pid) << "ProcessDumpTest025 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest025: end.";
}

/**
 * @tc.name: ProcessDumpTest026
 * @tc.desc: test get dump context
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest026: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    ucontext_t inputContext;
    inputContext.uc_flags = 1.0;
    processDump->SetContext(inputContext);
    ucontext_t outputContext = processDump->GetContext();
    EXPECT_EQ(true, outputContext.uc_flags == inputContext.uc_flags) << "GetContext Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest026: end.";
}

/**
 * @tc.name: ProcessDumpTest027
 * @tc.desc: test get dump context
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest027: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    ucontext_t inputContext;
    inputContext.uc_flags = 164.0;
    processDump->SetContext(inputContext);
    ucontext_t outputContext = processDump->GetContext();
    EXPECT_EQ(true, outputContext.uc_flags == inputContext.uc_flags) << "ProcessDumpTest027 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest027: end.";
}

/**
 * @tc.name: ProcessDumpTest028
 * @tc.desc: test get dump context
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest028, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest028: start.";
    std::shared_ptr<ProcessDumpRequest> processDump = std::make_shared<ProcessDumpRequest>();
    ucontext_t inputContext;
    inputContext.uc_flags = 3.0;
    processDump->SetContext(inputContext);
    ucontext_t outputContext = processDump->GetContext();
    EXPECT_EQ(true, outputContext.uc_flags == inputContext.uc_flags) << "ProcessDumpTest028 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest028: end.";
}

/**
 * @tc.name: ProcessDumpTest029
 * @tc.desc: test if signal info is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest029: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsAvailable();
    EXPECT_EQ(true, ret != true) << "ProcessDumpTest029 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest029: end.";
}

/**
 * @tc.name: ProcessDumpTest030
 * @tc.desc: test if addr is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest030: start.";
    int32_t input = -100;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsAddrAvailable();
    EXPECT_EQ(true, ret != true) << "ProcessDumpTest030 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest030: end.";
}

/**
 * @tc.name: ProcessDumpTest031
 * @tc.desc: test if pid is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest031, TestSize.Level2)
{
    int32_t input = 100;
    GTEST_LOG_(INFO) << "ProcessDumpTest031: start.";
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsPidAvailable();
    EXPECT_EQ(true, ret != true) << "ProcessDumpTest031 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest031: end.";
}

/**
 * @tc.name: ProcessDumpTest032
 * @tc.desc: test if pid is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest032: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    int32_t output = signal->GetSignal();
    EXPECT_EQ(true, output == input) << "ProcessDumpTest032 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest032: end.";
}

/**
 * @tc.name: ProcessDumpTest033
 * @tc.desc: test get process id
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest033, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest033: start.";
    int32_t pid = 1, tid = 1;
    ucontext_t context;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid, context);
    pid_t processID = thread->GetProcessId();
    GTEST_LOG_(INFO) << "ProcessDumpTest033: result = " << processID;
    EXPECT_EQ(true, pid == processID) << "ProcessDumpTest033 failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest033: end.";
}

/**
 * @tc.name: ProcessDumpTest034
 * @tc.desc: test get thread id
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest034, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest034: start.";
    int32_t pid = 243, tid = 243;
    ucontext_t context;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid, context);
    pid_t threadId = thread->GetThreadId();
    GTEST_LOG_(INFO) << "ProcessDumpTest034: result = " << threadId;
    EXPECT_EQ(true, tid == threadId) << "ProcessDumpTest034 failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest034: end.";
}

/**
 * @tc.name: ProcessDumpTest035
 * @tc.desc: test get thread name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest035, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest035: start.";
    int32_t pid = 243, tid = 243;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    pid_t threadId = thread->GetThreadId();
    EXPECT_EQ(true, threadId == tid) << "ProcessDumpTest035 failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest035: end.";
}

/**
 * @tc.name: ProcessDumpTest036
 * @tc.desc: test get thread name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest036, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest036: start.";
    int32_t pid = 1, tid = 1;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::string threadName = thread->GetThreadName();
    EXPECT_EQ(true, threadName != "");
    GTEST_LOG_(INFO) << "ProcessDumpTest036: result = " << threadName;
    GTEST_LOG_(INFO) << "ProcessDumpTest036: end.";
}

/**
 * @tc.name: ProcessDumpTest037
 * @tc.desc: test get DfxRegs
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest037, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest037: start.";
    int32_t pid = 243, tid = 243;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::shared_ptr<DfxRegs> inputrefs;
    thread->SetThreadRegs(inputrefs);
    std::shared_ptr<DfxRegs> outputrefs= thread->GetThreadRegs();
    EXPECT_EQ(true, inputrefs == outputrefs) << "ProcessDumpTest037 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest037: end.";
}

/**
 * @tc.name: ProcessDumpTest038
 * @tc.desc: test get DfxFrame
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest038, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest038: start.";
    int32_t pid = 243, tid = 243;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::shared_ptr<DfxFrame> outputrefs= thread->GetAvailableFrame();
    EXPECT_EQ(true, outputrefs != nullptr) << "ProcessDumpTest038 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest038: end.";
}

/**
 * @tc.name: ProcessDumpTest039
 * @tc.desc: test UnwindProcess
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest039, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest039: start.";
    std::shared_ptr<DfxProcess> process = std::make_shared<DfxProcess>();
    pid_t pid = GetProcessPid("accountmgr");
    pid_t tid = pid;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    const std::vector<std::shared_ptr<DfxThread>> threads = { thread };
    process->SetThreads(threads);
    thread->Attach();
    bool ret = DfxUnwindRemote::GetInstance().UnwindProcess(process);
    thread->Detach();
    EXPECT_EQ(true, ret) << "ProcessDumpTest039 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest039: end.";
}

/**
 * @tc.name: ProcessDumpTest040
 * @tc.desc: test UnwindThread
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest040, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest040: start.";
    std::shared_ptr<DfxProcess> process = std::make_shared<DfxProcess>();
    pid_t pid = GetProcessPid("accountmgr");
    pid_t tid = pid;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    thread->Attach();
    bool ret = DfxUnwindRemote::GetInstance().UnwindThread(process, thread);
    thread->Detach();
    EXPECT_EQ(true, ret) << "ProcessDumpTest040 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest040: end.";
}
}
