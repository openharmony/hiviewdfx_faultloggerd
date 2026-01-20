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

#include "dfx_test_util.h"
#include "dfx_util.h"

#include "coredump_buffer_writer.h"
#include "coredump_config_manager.h"
#include "coredump_controller.h"
#include "dfx_dump_request.h"
#include "coredump_mapping_manager.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxCoredumpUtilTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: BufferWriter001
 * @tc.desc: test BufferWriter write
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, BufferWriter001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter001: start.";

    CoredumpBufferWriter bw(nullptr, 0);
    char data[] = "test data";
    auto ret = bw.Write(data, sizeof(data));
    EXPECT_FALSE(ret);

    std::vector<char>buf(20);
    CoredumpBufferWriter bw1(buf.data(), buf.size());
    EXPECT_TRUE(bw1.Write(data, sizeof(data)));
    EXPECT_FALSE(bw1.Write(data, 100));
    GTEST_LOG_(INFO) << "BufferWriter001: end.";
}

/**
 * @tc.name: BufferWriter002
 * @tc.desc: test ProgramSegmentHeaderWriter Advance
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, BufferWriter002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter002: start.";

    CoredumpBufferWriter bw(nullptr, 0);
    auto ret = bw.Advance(10);
    EXPECT_FALSE(ret);

    std::vector<char>buf(20);
    CoredumpBufferWriter bw1(buf.data(), buf.size());
    ret = bw1.Advance(5);
    EXPECT_TRUE(ret);

    GTEST_LOG_(INFO) << "BufferWriter002: end.";
}

/**
 * @tc.name: BufferWriter003
 * @tc.desc: test ProgramSegmentHeaderWriter WriteAt
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, BufferWriter003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter003: start.";

    char data[] = "test data";

    CoredumpBufferWriter bw(nullptr, 0);
    auto ret = bw.WriteAt(data, sizeof(data), 0);
    EXPECT_FALSE(ret);

    std::vector<char>buf(20);
    CoredumpBufferWriter bw1(buf.data(), buf.size());

    ret = bw1.WriteAt(data, sizeof(data), 0);
    EXPECT_TRUE(ret);

    ret = bw1.WriteAt(data, sizeof(data), 100);
    EXPECT_FALSE(ret);

    size_t s1 = SIZE_MAX;
    ret = bw1.WriteAt(data, s1, s1);
    EXPECT_FALSE(ret);

    GTEST_LOG_(INFO) << "BufferWriter003: end.";
}

/**
 * @tc.name: BufferWriter004
 * @tc.desc: test ProgramSegmentHeaderWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, BufferWriter004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter004: start.";

    CoredumpBufferWriter bw(nullptr, 0);

    char data[] = "test data";

    auto ret = bw.SetCurrent(data);
    EXPECT_FALSE(ret);

    std::vector<char>buf(20);
    CoredumpBufferWriter bw1(buf.data(), buf.size());

    ret = bw1.SetCurrent(buf.data() + buf.size() - 5);
    EXPECT_TRUE(ret);
    ret = bw1.SetCurrent(buf.data() + buf.size() + 5);
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "BufferWriter004: end.";
}

/**
 * @tc.name: BufferWriter005
 * @tc.desc: test ProgramSegmentHeaderWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, BufferWriter005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter005: start.";
    std::vector<char>buf(20);
    CoredumpBufferWriter bw1(buf.data(), buf.size());

    size_t size = 5;
    bw1.Advance(size);
    EXPECT_EQ(bw1.GetOffset(), size);
    bw1.Reset();
    EXPECT_EQ(bw1.GetBase(), bw1.GetCurrent());
    GTEST_LOG_(INFO) << "BufferWriter005: end.";
}

/**
 * @tc.name: CoredumpConfigManager001
 * @tc.desc: test ProgramSegmentHeaderWriter
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpConfigManager001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager001: start.";
    auto& configManager = CoredumpConfigManager::GetInstance();

    configManager.LoadCoredumpConfig("FULL");

    GTEST_LOG_(INFO) << "CoredumpConfigManager001: end.";
}

/**
 * @tc.name: CoredumpConfigManager002
 * @tc.desc: test CoredumpConfigManager LoadProfileFromJson
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpConfigManager002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager002: start.";
    auto& configManager = CoredumpConfigManager::GetInstance();
    std::string content = "{invalid";
    auto ret = configManager.LoadProfileFromJson(content, "default");
    EXPECT_FALSE(ret);

    content = R"({
        "Profiles": {
        }
    })";

    ret = configManager.LoadProfileFromJson(content, "default");
    EXPECT_FALSE(ret);

    content = R"({
        "coredumpProfiles": {
            "FULL": {
                "enabled": true
            }
        }
    })";

    ret = configManager.LoadProfileFromJson(content, "default");
    EXPECT_FALSE(ret);

    ret = configManager.LoadProfileFromJson(content, "FULL");
    EXPECT_TRUE(ret);

    GTEST_LOG_(INFO) << "CoredumpConfigManager002: end.";
}

/**
 * @tc.name: CoredumpConfigManager003
 * @tc.desc: test CoredumpConfigManager LoadProfileFromFile
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpConfigManager003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager003: start.";
    auto& configManager = CoredumpConfigManager::GetInstance();
    std::string path = "/system/etc/fault_coredump.json";
    auto ret = configManager.LoadProfileFromFile(path, "FULL");
    EXPECT_TRUE(ret);

    path = "/system/etc/fault_coredump3.json";
    ret = configManager.LoadProfileFromFile(path, "default");
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpConfigManager003: end.";
}

/**
 * @tc.name: CoredumpController002
 * @tc.desc: test CoredumpController VerifyProcess
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpController002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpController002: start.";
    auto& val = CoredumpConfigManager::GetInstance().dumpConfig_.coredumpSwitch;
    auto oldVal = val;
    val = false;
    EXPECT_FALSE(CoredumpController::VerifyProcess());
    val = true;
    EXPECT_TRUE(CoredumpController::VerifyProcess());
    val = oldVal;
    GTEST_LOG_(INFO) << "CoredumpController002: end.";
}

/**
 * @tc.name: CoredumpController003
 * @tc.desc: test CoredumpController IsCoredumpAllowed
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpController003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpController003: start.";
    ProcessDumpRequest request;

    request.siginfo.si_signo = 11;
    bool ret = CoredumpController::IsCoredumpAllowed(request);
    EXPECT_FALSE(ret);

    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 3;
    ret = CoredumpController::IsCoredumpAllowed(request);
    EXPECT_TRUE(ret);

    request.siginfo.si_signo = 6;
    ret = CoredumpController::IsCoredumpAllowed(request);
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpController003: end.";
}

/**
 * @tc.name: CoredumpController004
 * @tc.desc: test coredump IsCoredumpSignal and IsHwasanCoredumpEnabled function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpController004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpController004: start.";

    ProcessDumpRequest request;
    bool ret = CoredumpController::IsCoredumpSignal(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 6;
    ret = CoredumpController::IsCoredumpSignal(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 0;
    request.siginfo.si_code = 3;
    ret = CoredumpController::IsCoredumpSignal(request);
    ASSERT_TRUE(!ret);

    request.siginfo.si_signo = 42;
    request.siginfo.si_code = 3;
    ret = CoredumpController::IsCoredumpSignal(request);
    ASSERT_TRUE(ret);

    ret = CoredumpController::IsHwasanCoredumpEnabled();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "CoredumpController004: end.";
}

/**
 * @tc.name: CoredumpController005
 * @tc.desc: test coredump VerifyTrustList function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpController005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpController005: start.";
    bool ret = CoredumpController::VerifyTrustList("");
    EXPECT_FALSE(ret);
    ret = CoredumpController::VerifyTrustList("abc");
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpController005: end.";
}

/**
 * @tc.name: CoredumpMappingManager001
 * @tc.desc: test coredump ObtainDumpRegion function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpMappingManager001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpMappingManager001: start.";
    auto& instance = CoredumpMappingManager::GetInstance();
    std::string line = "5a0eb08000-5a0eb09000 r-xp 00007000 00:00 0            /system/lib/test.z.so";
    DumpMemoryRegions region;
    instance.ObtainDumpRegion(line, region);
    ASSERT_EQ(region.memorySizeHex, 0x1000);
    bool ret = instance.IsHwAsanProcess();
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpMappingManager001: end.";
}

/**
 * @tc.name: CoredumpMappingManager002
 * @tc.desc: test coredump ObtainDumpRegion function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpMappingManager002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpMappingManager002: start.";
    auto& instance = CoredumpMappingManager::GetInstance();
    std::string line = "5a0eb08000-5a0eb09000 r-xp 00007000 00:00 0            /system/lib/test.z.so";
    DumpMemoryRegions region;
    instance.ObtainDumpRegion(line, region);
    auto ret = CoredumpMappingManager::ShouldIncludeRegion(region);
    EXPECT_TRUE(ret);

    line = "5a0eb08000-5a0eb09000 --xp 00007000 00:00 0            /system/lib/test.z.so";
    instance.ObtainDumpRegion(line, region);
    ret = CoredumpMappingManager::ShouldIncludeRegion(region);
    EXPECT_FALSE(ret);

    GTEST_LOG_(INFO) << "CoredumpMappingManager002: end.";
}

/**
 * @tc.name: CoredumpJsonUtil001
 * @tc.desc: test coredump ObtainDumpRegion function
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoredumpUtilTest, CoredumpJsonUtil001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpJsonUtil001: start.";

    const char* jsonText = R"({
        "valid_int": 42,
        "string_value": "not_an_int",
        "nested": { "inner": 100 }
    })";

    std::unique_ptr<cJSON, CJSONDeleter> root(cJSON_Parse(jsonText));
    EXPECT_TRUE(root);
    EXPECT_EQ(CoredumpJsonUtil::GetIntSafe(root.get(), "valid_int", -1), 42);
    EXPECT_EQ(CoredumpJsonUtil::GetIntSafe(nullptr, "valid_int", -1), -1);
    EXPECT_EQ(CoredumpJsonUtil::GetIntSafe(root.get(), nullptr, -1), -1);
    EXPECT_EQ(CoredumpJsonUtil::GetIntSafe(root.get(), "valid_int", -1), 42);
    EXPECT_EQ(CoredumpJsonUtil::GetIntSafe(root.get(), "missing_key", -1), -1);
    EXPECT_EQ(CoredumpJsonUtil::GetIntSafe(root.get(), "string_value", 999), 999);

    EXPECT_EQ(CoredumpJsonUtil::GetBoolSafe(nullptr, "valid_int", false), false);
    EXPECT_EQ(CoredumpJsonUtil::GetBoolSafe(root.get(), nullptr, false), false);

    EXPECT_EQ(CoredumpJsonUtil::GetStrSafe(nullptr, "string_value", ""), "");
    EXPECT_EQ(CoredumpJsonUtil::GetStrSafe(root.get(), "valid_int", ""), "");
    EXPECT_EQ(CoredumpJsonUtil::GetStrSafe(root.get(), nullptr, ""), "");

    GTEST_LOG_(INFO) << "CoredumpJsonUtil001: end.";
}
}
