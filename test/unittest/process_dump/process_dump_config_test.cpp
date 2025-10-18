/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "decorative_dump_info.h"
#include "process_dump_config.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ProcessDumpConfigTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: ProcessDumpConfigTest001
 * @tc.desc: test process dump config
 * @tc.type: FUNC
 */
HWTEST_F(ProcessDumpConfigTest, ProcessDumpConfigTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpConfigTest001: start.";
    auto config = ProcessDumpConfig::GetInstance().GetConfig();
    ASSERT_EQ(config.extendPcLrPrinting, false);
    ASSERT_EQ(config.faultStackHighAddrStep, 512);
    ASSERT_EQ(config.faultStackLowAddrStep, 32);
    ASSERT_EQ(config.maxFrameNums, 256);
    ASSERT_EQ(config.logFileCutoffSizeBytes, 0);
    ASSERT_EQ(config.simplifyVmaPrinting, false);
    ASSERT_EQ(config.reservedParseSymbolTime, 100);
    std::vector<std::string> dumpInfoCppCrash = {
        "KeyThreadDumpInfo", "DumpInfoHeader", "SubmitterStack",
        "Registers", "ExtraCrashInfo", "OtherThreadDumpInfo",
        "MemoryNearRegister", "FaultStack", "Maps", "OpenFiles",
    };
    ASSERT_EQ(dumpInfoCppCrash, config.dumpInfo[ProcessDumpType::DUMP_TYPE_CPP_CRASH]);

    std::vector<std::string> dumpInfoDumpCatch = {
        "KeyThreadDumpInfo", "DumpInfoHeader", "OtherThreadDumpInfo",
    };
    ASSERT_EQ(dumpInfoDumpCatch, config.dumpInfo[ProcessDumpType::DUMP_TYPE_DUMP_CATCH]);

    std::vector<std::string> dumpInfoMemLeak = {
        "KeyThreadDumpInfo", "DumpInfoHeader", "Registers",
        "MemoryNearRegister", "FaultStack", "Maps",
    };
    ASSERT_EQ(dumpInfoMemLeak, config.dumpInfo[ProcessDumpType::DUMP_TYPE_MEM_LEAK]);

    std::vector<std::string> dumpInfoFdSan = {
        "KeyThreadDumpInfo", "DumpInfoHeader", "Registers",
        "MemoryNearRegister", "FaultStack", "Maps", "OpenFiles",
    };
    ASSERT_EQ(dumpInfoFdSan, config.dumpInfo[ProcessDumpType::DUMP_TYPE_FDSAN]);

    std::vector<std::string> dumpInfoJeMalloc = {
        "KeyThreadDumpInfo", "DumpInfoHeader", "Registers",
        "MemoryNearRegister", "FaultStack", "Maps",
    };
    ASSERT_EQ(dumpInfoJeMalloc, config.dumpInfo[ProcessDumpType::DUMP_TYPE_JEMALLOC]);

    std::vector<std::string> dumpInfoBadFd = {
        "KeyThreadDumpInfo", "DumpInfoHeader", "Registers",
        "MemoryNearRegister", "FaultStack", "Maps", "OpenFiles",
    };
    ASSERT_EQ(dumpInfoBadFd, config.dumpInfo[ProcessDumpType::DUMP_TYPE_BADFD]);

    std::vector<std::string> dumpInfoCoreDump = {
        "KeyThreadDumpInfo", "Registers", "OtherThreadDumpInfo",
    };
    ASSERT_EQ(dumpInfoCoreDump, config.dumpInfo[ProcessDumpType::DUMP_TYPE_COREDUMP]);
    GTEST_LOG_(INFO) << "ProcessDumpConfigTest001: end.";
}
}
