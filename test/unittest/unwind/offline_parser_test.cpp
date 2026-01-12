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
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include "dfx_offline_parser.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxOfflineParserTest : public testing::Test {};

/**
 * @tc.name: DfxOfflineParserTest001
 * @tc.desc: test IsJsFrame
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest001: start.";
    DfxFrame frame;
    frame.mapName = "xxx.hap";
    bool isJsFrame = DfxOfflineParser::IsJsFrame(frame);
    ASSERT_TRUE(isJsFrame);
    frame.mapName = "xxx.so";
    isJsFrame = DfxOfflineParser::IsJsFrame(frame);
    ASSERT_FALSE(isJsFrame);
    GTEST_LOG_(INFO) << "DfxOfflineParserTest001: end.";
}

/**
 * @tc.name: DfxOfflineParserTest002
 * @tc.desc: test GetBundlePath with empty bundlename
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest002: start.";
    DfxOfflineParser parser("");
    std::string originPath = "/system/lib/ld-musl-arm.so.1";
    std::string realPath = parser.GetBundlePath(originPath);
    EXPECT_EQ(realPath, originPath);
    originPath = "/data/storage/el1/bundle/libs/arm/xxx.so";
    realPath = parser.GetBundlePath(originPath);
    EXPECT_EQ(realPath, originPath);
    GTEST_LOG_(INFO) << "DfxOfflineParserTest002: end.";
}

/**
 * @tc.name: DfxOfflineParserTest003
 * @tc.desc: test GetBundlePath with bundlename
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest003: start.";
    DfxOfflineParser parser("testhap");
    std::string originPath = "/system/lib/ld-musl-arm.so.1";
    std::string realPath = parser.GetBundlePath(originPath);
    EXPECT_EQ(realPath, originPath);
    originPath = "/data/storage/el1/bundle/libs/arm/xxx.so";
    realPath = parser.GetBundlePath(originPath);
    EXPECT_EQ(realPath, "/data/app/el1/bundle/public/testhap/libs/arm/xxx.so") << realPath;
    GTEST_LOG_(INFO) << "DfxOfflineParserTest003: end.";
}

/**
 * @tc.name: DfxOfflineParserTest004
 * @tc.desc: test GetElfForFrame with dfxmaps nullptr
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest004: start.";
    DfxOfflineParser parser("testhap");
    parser.dfxMaps_ = nullptr;
    DfxFrame frame;
    auto elf = parser.GetElfForFrame(frame);
    EXPECT_EQ(elf, nullptr);
    GTEST_LOG_(INFO) << "DfxOfflineParserTest004: end.";
}

/**
 * @tc.name: DfxOfflineParserTest005
 * @tc.desc: test GetElfForFrame with find the same map in dfxmaps
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest005: start.";
    DfxOfflineParser parser("testhap");
#if defined(__aarch64__)
    std::string soName = "/system/lib64/chipset-sdk-sp/libunwinder.z.so";
#else
    std::string soName = "/system/lib/chipset-sdk-sp/libunwinder.z.so";
#endif
    DfxFrame frame1;
    frame1.mapName = soName;
    auto elf1 = parser.GetElfForFrame(frame1);
    DfxFrame frame2;
    frame2.mapName = soName;
    auto elf2 = parser.GetElfForFrame(frame2);
    EXPECT_EQ(elf1, elf2);
    GTEST_LOG_(INFO) << "DfxOfflineParserTest005: end.";
}

/**
 * @tc.name: DfxOfflineParserTest006
 * @tc.desc: test GetElfForFrame with invalid mapname
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest006: start.";
    DfxOfflineParser parser("testhap");
    DfxFrame frame;
    frame.mapName = "invalidmapname";
    auto elf = parser.GetElfForFrame(frame);
    EXPECT_EQ(elf, nullptr);
    GTEST_LOG_(INFO) << "DfxOfflineParserTest006: end.";
}

/**
 * @tc.name: DfxOfflineParserTest007
 * @tc.desc: test GetElfForFrame with invalid mapname in 10 times
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest007: start.";
    DfxOfflineParser parser("testhap");
    DfxFrame frame;
    frame.mapName = "invalidmapname.so";
    std::shared_ptr<DfxElf> elf = nullptr;
    for (int i = 0; i < 10; i++) {
        elf = parser.GetElfForFrame(frame);
        EXPECT_EQ(elf, nullptr);
    }
    GTEST_LOG_(INFO) << "DfxOfflineParserTest007: end.";
}

/**
 * @tc.name: DfxOfflineParserTest008
 * @tc.desc: test parse symbol for arkweb so
 * @tc.type: FUNC
 */
HWTEST_F(DfxOfflineParserTest, DfxOfflineParserTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxOfflineParserTest008: start.";
    std::string arkwebSo = "/data/storage/el1/bundle/arkwebcore/libs/arm64/libarkweb_engine.so";
    bool isFileExist = access(arkwebSo.c_str(), F_OK) == 0;
    if (isFileExist) {
        DfxOfflineParser parser("testhap");
        DfxFrame frame;
        frame.mapName = arkwebSo;
        parser.ParseSymbolWithFrame(frame);
        ASSERT_FALSE(frame.buildId.empty());
    } else {
        ASSERT_TRUE(!isFileExist);
    }
    GTEST_LOG_(INFO) << "DfxOfflineParserTest008: end.";
}
} // namespace HiviewDFX
} // namespace OHOS