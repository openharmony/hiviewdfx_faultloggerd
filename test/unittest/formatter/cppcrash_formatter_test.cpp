/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "cppcrash_info_collector.h"
#include "cppcrash_formatter.h"
#include "dfx_frame.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX;

namespace OHOS {
namespace HiviewDFX {

class CppCrashFormatterTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase()
    {
        CppCrashInfoCollector::Instance().Reset();
    }
    void SetUp()
    {
        CppCrashInfoCollector::Instance().Reset();
        CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    }
    void TearDown() {}
};

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetBasicInfo
 * @tc.desc: Test setting and getting basic info fields of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetBasicInfo, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetBuildInfo("OpenHarmony 5.0.0");
    EXPECT_EQ(collector.GetBuildInfo(), "OpenHarmony 5.0.0");
    
    collector.SetTimestamp("2026-04-29 10:00:00");
    EXPECT_EQ(collector.GetTimestamp(), "2026-04-29 10:00:00");
    
    collector.SetPid(12345);
    EXPECT_EQ(collector.GetPid(), 12345);
    
    collector.SetUid(1000);
    EXPECT_EQ(collector.GetUid(), 1000);
    
    collector.SetHiTraceId("0123456789abcdef");
    EXPECT_EQ(collector.GetHiTraceId(), "0123456789abcdef");
    
    collector.SetPname("test_process");
    EXPECT_EQ(collector.GetPname(), "test_process");
}

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetProcessInfo
 * @tc.desc: Test setting and getting process info fields of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetProcessInfo, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetProcessLifeTime("100s");
    EXPECT_EQ(collector.GetProcessLifeTime(), "100s");
    
    collector.SetProcessRssMeminfo("12345 6789");
    EXPECT_EQ(collector.GetProcessRssMeminfo(), "12345 6789");
    
    collector.SetReason("Signal:SIGSEGV(SEGV_MAPERR)");
    EXPECT_EQ(collector.GetReason(), "Signal:SIGSEGV(SEGV_MAPERR)");
}

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetSignalInfo
 * @tc.desc: Test setting and getting signal info of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetSignalInfo, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetSignal(11, 1, "0x12345678");
    EXPECT_EQ(collector.GetSignalSigno(), 11);
    EXPECT_EQ(collector.GetSignalCode(), 1);
    EXPECT_EQ(collector.GetSignalAddress(), "0x12345678");
}

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetLastFatalMessage
 * @tc.desc: Test setting and getting last fatal message of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetLastFatalMessage, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetLastFatalMessage("Fatal error occurred");
    EXPECT_EQ(collector.GetLastFatalMessage(), "Fatal error occurred");
}

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetKeyThread
 * @tc.desc: Test setting and getting key thread info of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetKeyThread, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    std::vector<DfxFrame> frames;
    DfxFrame frame;
    frame.relPc = 0x12345678;
    frame.funcName = "testFunc";
    frame.funcOffset = 100;
    frame.mapName = "/system/lib/libtest.so";
    frame.buildId = "abc123";
    frames.push_back(frame);
    
    collector.SetKeyThread("main", 12345, frames);
    
    const ThreadInfo& keyThread = collector.GetKeyThread();
    EXPECT_EQ(keyThread.threadName, "main");
    EXPECT_EQ(keyThread.tid, 12345);
    EXPECT_EQ(keyThread.frames.size(), 1u);
}

/**
 * @tc.name: CppCrashInfoCollectorAddOtherThread
 * @tc.desc: Test adding other thread info to CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorAddOtherThread, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    std::vector<DfxFrame> frames;
    DfxFrame frame;
    frame.relPc = 0x87654321;
    frame.funcName = "otherFunc";
    frames.push_back(frame);
    
    collector.AddOtherThread("thread1", 12346, frames);
    
    const std::vector<ThreadInfo>& otherThreads = collector.GetOtherThreads();
    EXPECT_EQ(otherThreads.size(), 1u);
    EXPECT_EQ(otherThreads[0].threadName, "thread1");
    EXPECT_EQ(otherThreads[0].tid, 12346);
}

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetOptionalInfo
 * @tc.desc: Test setting and getting optional info fields of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetOptionalInfo, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetSubmitterStacktrace("#00 pc 1234 /system/lib/libtest.so");
    EXPECT_EQ(collector.GetSubmitterStacktrace(), "#00 pc 1234 /system/lib/libtest.so");
    
    collector.SetRegisters("pc:0x12345678 sp:0x87654321");
    EXPECT_EQ(collector.GetRegisters(), "pc:0x12345678 sp:0x87654321");
    
    collector.SetExtraCrashInfo("Extra info");
    EXPECT_EQ(collector.GetExtraCrashInfo(), "Extra info");
    
    collector.SetMemoryNearRegister("Memory content");
    EXPECT_EQ(collector.GetMemoryNearRegister(), "Memory content");
    
    collector.SetFaultStack("Stack content");
    EXPECT_EQ(collector.GetFaultStack(), "Stack content");
    
    collector.SetMaps("Maps content");
    EXPECT_EQ(collector.GetMaps(), "Maps content");
    
    collector.SetOpenFiles("fd 0: /dev/null");
    EXPECT_EQ(collector.GetOpenFiles(), "fd 0: /dev/null");
}

/**
 * @tc.name: CppCrashInfoCollectorSetAndGetConfigInfo
 * @tc.desc: Test setting and getting config info fields of CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorSetAndGetConfigInfo, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetExtendPcLrPrinting(true);
    EXPECT_TRUE(collector.GetExtendPcLrPrinting());
    
    collector.SetLogCutOffSizeStr("1024B\n");
    EXPECT_EQ(collector.GetLogCutOffSizeStr(), "1024B\n");
    
    collector.SetSimplifyVmaPrinting(true);
    EXPECT_TRUE(collector.GetSimplifyVmaPrinting());
    
    collector.SetMergeAppLog(true);
    EXPECT_TRUE(collector.GetMergeAppLog());
    
    collector.SetMinidumpLog(true);
    EXPECT_TRUE(collector.GetMinidumpLog());
}

/**
 * @tc.name: CppCrashInfoCollectorReset
 * @tc.desc: Test Reset method of CppCrashInfoCollector to clear all fields
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashInfoCollectorReset, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    collector.SetBuildInfo("test");
    collector.SetPid(12345);
    collector.SetKeyThread("main", 12345, {});
    collector.SetExtendPcLrPrinting(true);
    
    collector.Reset();
    
    EXPECT_EQ(collector.GetBuildInfo(), "");
    EXPECT_EQ(collector.GetPid(), 0);
    EXPECT_EQ(collector.GetKeyThread().threadName, "");
    EXPECT_FALSE(collector.GetExtendPcLrPrinting());
}

/**
 * @tc.name: CppCrashJsonFormatterFormatBasic
 * @tc.desc: Test basic JSON formatting with build info and process info
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashJsonFormatterFormatBasic, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    collector.SetBuildInfo("OpenHarmony 5.0.0");
    collector.SetPid(12345);
    collector.SetUid(1000);
    collector.SetPname("test_process");
    
    CppCrashJsonFormatter formatter;
    std::string jsonStr = formatter.FormatCrashInfo();
    
    EXPECT_TRUE(jsonStr.find("\"JSON_VERSION\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"BUILD_INFO\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("OpenHarmony 5.0.0") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"PID\":12345") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"UID\":1000") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"PNAME\"") != std::string::npos);
}

/**
 * @tc.name: CppCrashJsonFormatterFormatWithSignal
 * @tc.desc: Test JSON formatting with signal info
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashJsonFormatterFormatWithSignal, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    collector.SetSignal(11, 1, "0x12345678");
    
    CppCrashJsonFormatter formatter;
    std::string jsonStr = formatter.FormatCrashInfo();
    
    EXPECT_TRUE(jsonStr.find("\"SIGNAL\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"signo\":11") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"code\":1") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("0x12345678") != std::string::npos);
}

/**
 * @tc.name: CppCrashJsonFormatterFormatWithConfig
 * @tc.desc: Test JSON formatting with ENABLED_APP_LOG_CONFIG object
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashJsonFormatterFormatWithConfig, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    collector.SetExtendPcLrPrinting(true);
    collector.SetLogCutOffSizeStr("1024B\n");
    collector.SetSimplifyVmaPrinting(true);
    
    CppCrashJsonFormatter formatter;
    std::string jsonStr = formatter.FormatCrashInfo();
    
    EXPECT_TRUE(jsonStr.find("\"ENABLED_APP_LOG_CONFIG\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"EXTEND_PC_LR_PRINTING\":true") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"LOG_CUT_OFF_SIZE\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"SIMPLIFY_MAPS_PRINTING\":true") != std::string::npos);
}

/**
 * @tc.name: CppCrashJsonFormatterFormatWithKeyThread
 * @tc.desc: Test JSON formatting with KEY_THREAD_INFO object
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashJsonFormatterFormatWithKeyThread, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    std::vector<DfxFrame> frames;
    DfxFrame frame;
    frame.relPc = 0x12345678;
    frame.funcName = "testFunc";
    frame.funcOffset = 100;
    frame.mapName = "/system/lib/libtest.so";
    frame.buildId = "abc123";
    frames.push_back(frame);
    
    collector.SetKeyThread("main", 12345, frames);
    
    CppCrashJsonFormatter formatter;
    std::string jsonStr = formatter.FormatCrashInfo();
    
    EXPECT_TRUE(jsonStr.find("\"KEY_THREAD_INFO\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"thread_name\":\"main\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"tid\":12345") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"frames\"") != std::string::npos);
}

/**
 * @tc.name: CppCrashJsonFormatterFormatWithJsFrame
 * @tc.desc: Test JSON formatting with JS frame in KEY_THREAD_INFO
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashJsonFormatterFormatWithJsFrame, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    std::vector<DfxFrame> frames;
    DfxFrame jsFrame;
    jsFrame.isJsFrame = true;
    jsFrame.mapName = "/path/to/test.js";
    jsFrame.packageName = "com.test.app";
    jsFrame.funcName = "jsFunc";
    jsFrame.line = 123;
    jsFrame.column = 456;
    frames.push_back(jsFrame);
    
    collector.SetKeyThread("main", 12345, frames);
    
    CppCrashJsonFormatter formatter;
    std::string jsonStr = formatter.FormatCrashInfo();
    
    EXPECT_TRUE(jsonStr.find("\"packageName\":\"com.test.app\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"symbol\":\"jsFunc\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"line\":123") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"column\":456") != std::string::npos);
}

/**
 * @tc.name: CppCrashJsonFormatterFormatWithOtherThread
 * @tc.desc: Test JSON formatting with OTHER_THREAD_INFO array
 * @tc.type: FUNC
 */
HWTEST_F(CppCrashFormatterTest, CppCrashJsonFormatterFormatWithOtherThread, TestSize.Level0)
{
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    
    std::vector<DfxFrame> frames1;
    DfxFrame frame1;
    frame1.relPc = 0x11111111;
    frame1.funcName = "thread1Func";
    frames1.push_back(frame1);
    
    collector.AddOtherThread("thread1", 12346, frames1);
    
    std::vector<DfxFrame> frames2;
    DfxFrame frame2;
    frame2.relPc = 0x22222222;
    frame2.funcName = "thread2Func";
    frames2.push_back(frame2);
    
    collector.AddOtherThread("thread2", 12347, frames2);
    
    CppCrashJsonFormatter formatter;
    std::string jsonStr = formatter.FormatCrashInfo();
    
    EXPECT_TRUE(jsonStr.find("\"OTHER_THREAD_INFO\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"thread_name\":\"thread1\"") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("\"thread_name\":\"thread2\"") != std::string::npos);
}

} // namespace HiviewDFX
} // namespace OHOS