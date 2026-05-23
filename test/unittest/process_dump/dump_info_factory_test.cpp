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
#include <memory>
#include <string>

#include "decorative_dump_info.h"
#include "dump_info_factory.h"
#include "key_thread_dump_info.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class DumpInfoFactoryTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: DumpInfoFactoryTest001
 * @tc.desc: test DumpInfoFactory registering and creating instance
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoFactoryTest, DumpInfoFactory_RegisterAndCreate_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoFactory_RegisterAndCreate_001: start.";
    auto& factory = DumpInfoFactory::GetInstance();
    factory.RegisterClass("KeyThreadDumpInfo", KeyThreadDumpInfo::CreateInstance);
    auto obj = factory.CreateObject("KeyThreadDumpInfo");
    ASSERT_TRUE(obj != nullptr);
    GTEST_LOG_(INFO) << "DumpInfoFactory_RegisterAndCreate_001: end.";
}

/**
 * @tc.name: DumpInfoFactoryTest002
 * @tc.desc: test DumpInfoFactory creating unknown class returns nullptr
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoFactoryTest, DumpInfoFactory_CreateUnknown_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoFactory_CreateUnknown_002: start.";
    auto& factory = DumpInfoFactory::GetInstance();
    auto obj = factory.CreateObject("UnknownClassName");
    ASSERT_TRUE(obj == nullptr);
    GTEST_LOG_(INFO) << "DumpInfoFactory_CreateUnknown_002: end.";
}

/**
 * @tc.name: DumpInfoFactoryTest003
 * @tc.desc: test DumpInfoFactory registering duplicate class name
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoFactoryTest, DumpInfoFactory_RegisterDuplicate_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoFactory_RegisterDuplicate_003: start.";
    auto& factory = DumpInfoFactory::GetInstance();
    factory.RegisterClass("KeyThreadDumpInfo", KeyThreadDumpInfo::CreateInstance);
    factory.RegisterClass("KeyThreadDumpInfo", KeyThreadDumpInfo::CreateInstance);
    auto obj = factory.CreateObject("KeyThreadDumpInfo");
    ASSERT_TRUE(obj != nullptr);
    GTEST_LOG_(INFO) << "DumpInfoFactory_RegisterDuplicate_003: end.";
}

/**
 * @tc.name: DumpInfoFactoryTest004
 * @tc.desc: test DumpInfoFactory creating all registered dump info classes
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoFactoryTest, DumpInfoFactory_CreateAllRegistered_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoFactory_CreateAllRegistered_004: start.";
    auto& factory = DumpInfoFactory::GetInstance();
    std::vector<std::string> classNames = {
        "KeyThreadDumpInfo",
        "DumpInfoHeader",
        "Registers",
        "FaultStack",
        "Maps",
        "MemoryNearRegister",
        "OtherThreadDumpInfo",
        "OpenFiles",
    };
    for (const auto& name : classNames) {
        auto obj = factory.CreateObject(name);
        ASSERT_TRUE(obj != nullptr) << "Failed to create: " << name;
    }
    GTEST_LOG_(INFO) << "DumpInfoFactory_CreateAllRegistered_004: end.";
}
}