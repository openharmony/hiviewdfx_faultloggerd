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

/* This files contains unit test for frame module. */

#include "process_frames_test.h"

#include <memory>

#include "dfx_frames.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

void ProcessFramesTest::SetUpTestCase(void)
{
}


void ProcessFramesTest::TearDownTestCase(void)
{
}

void ProcessFramesTest::SetUp(void)
{
}

void ProcessFramesTest::TearDown(void)
{
}

namespace {
/**
 * @tc.name: ProcessFrameTest001
 * @tc.desc: test get frame index
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest001: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    size_t index = 1;
    size_t frameIndex = 0;
    if (frames != nullptr) {
        frames->SetFrameIndex(index);
        frameIndex = frames->GetFrameIndex();
    }
    EXPECT_EQ(true, index == frameIndex) << "ProcessFrameTest001 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest001: end.";
}

/**
 * @tc.name: ProcessFrameTest002
 * @tc.desc: test get frame index
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest002: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    size_t index = 164;
    size_t frameIndex = 0;
    if (frames != nullptr) {
        frames->SetFrameIndex(index);
        frameIndex = frames->GetFrameIndex();
    }
    EXPECT_EQ(true, index == frameIndex) << "ProcessFrameTest002 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest002: end.";
}

/**
 * @tc.name: ProcessFrameTest003
 * @tc.desc: test get frame index
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest003: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    size_t index = -1;
    frames->SetFrameIndex(index);
    size_t frameIndex = frames->GetFrameIndex();
    EXPECT_EQ(true, index == frameIndex) << "ProcessFrameTest003 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest003: end.";
}

/**
 * @tc.name: ProcessFrameTest004
 * @tc.desc: test get frame func offset
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest004: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t offset = 1;
    frames->SetFrameFuncOffset(offset);
    uint64_t frameOffset = frames->GetFrameFuncOffset();
    EXPECT_EQ(true, offset == frameOffset) << "ProcessFrameTest004 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest004: end.";
}

/**
 * @tc.name: ProcessFrameTest005
 * @tc.desc: test get frame func offset
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest005: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t offset = 164;
    frames->SetFrameFuncOffset(offset);
    uint64_t frameOffset = frames->GetFrameFuncOffset();
    EXPECT_EQ(true, offset == frameOffset) << "ProcessFrameTest005 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest005: end.";
}

/**
 * @tc.name: ProcessFrameTest006
 * @tc.desc: test get frame func offset
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest006: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t offset = -1;
    frames->SetFrameFuncOffset(offset);
    uint64_t frameOffset = frames->GetFrameFuncOffset();
    EXPECT_EQ(true, offset == frameOffset) << "ProcessFrameTest006 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest006: end.";
}

/**
 * @tc.name: ProcessFrameTest007
 * @tc.desc: test get frame pc
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest007: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t pc = 1;
    uint64_t framePc = 0;
    if (frames != nullptr) {
        frames->SetFramePc(pc);
        framePc = frames->GetFramePc();
    }
    EXPECT_EQ(true, pc == framePc) << "ProcessFrameTest007 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest007: end.";
}

/**
 * @tc.name: ProcessFrameTest008
 * @tc.desc: test get frame pc
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest008: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t pc = 164;
    frames->SetFramePc(pc);
    uint64_t framePc = frames->GetFramePc();
    EXPECT_EQ(true, pc == framePc) << "ProcessFrameTest008 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest008: end.";
}

/**
 * @tc.name: ProcessFrameTest009
 * @tc.desc: test get frame pc
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest009: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t pc = -1;
    frames->SetFramePc(pc);
    uint64_t framePc = frames->GetFramePc();
    EXPECT_EQ(true, pc == framePc) << "ProcessFrameTest009 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest009: end.";
}

/**
 * @tc.name: ProcessFrameTest010
 * @tc.desc: test get frame sp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest010: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t sp = 1;
    frames->SetFrameSp(sp);
    uint64_t frameSp = frames->GetFrameSp();
    EXPECT_EQ(true, sp == frameSp) << "ProcessFrameTest010 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest010: end.";
}

/**
 * @tc.name: ProcessFrameTest011
 * @tc.desc: test get frame sp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest011: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t sp = 164;
    uint64_t frameSp = 0;
    if (frames != nullptr) {
        frames->SetFrameSp(sp);
        frameSp = frames->GetFrameSp();
    }
    EXPECT_EQ(true, sp == frameSp) << "ProcessFrameTest011 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest011: end.";
}

/**
 * @tc.name: ProcessFrameTest012
 * @tc.desc: test get frame sp
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest012: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t sp = -1;
    frames->SetFrameSp(sp);
    uint64_t frameSp = frames->GetFrameSp();
    EXPECT_EQ(true, sp == frameSp) << "ProcessFrameTest012 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest012: end.";
}

/**
 * @tc.name: ProcessFrameTest013
 * @tc.desc: test get frame relative pc
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest013: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t relativePc = 1;
    uint64_t frameRelativePc = 0;
    if (frames != nullptr) {
        frames->SetFrameRelativePc(relativePc);
        frameRelativePc = frames->GetFrameRelativePc();
    }
    EXPECT_EQ(true, relativePc == frameRelativePc) << "ProcessFrameTest013 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest013: end.";
}

/**
 * @tc.name: ProcessFrameTest014
 * @tc.desc: test get frame relative pc
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest014: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t relativePc = 164;
    frames->SetFrameRelativePc(relativePc);
    uint64_t frameRelativePc = frames->GetFrameRelativePc();
    EXPECT_EQ(true, relativePc == frameRelativePc) << "ProcessFrameTest014 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest014: end.";
}

/**
 * @tc.name: ProcessFrameTest015
 * @tc.desc: test get frame relative pc
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest015: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    uint64_t relativePc = -1;
    frames->SetFrameRelativePc(relativePc);
    uint64_t frameRelativePc = frames->GetFrameRelativePc();
    EXPECT_EQ(true, relativePc == frameRelativePc) << "ProcessFrameTest015 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest015: end.";
}

/**
 * @tc.name: ProcessFrameTest016
 * @tc.desc: test get frame func name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest016: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    std::string name = "zhangsan";
    std::string frameName = "";
    if (frames != nullptr) {
        frames->SetFrameFuncName(name);
        frameName = frames->GetFrameFuncName();
    }
    EXPECT_EQ(true, name == frameName) << "ProcessFrameTest016 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest016: end.";
}

/**
 * @tc.name: ProcessFrameTest017
 * @tc.desc: test get frame func name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest017: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    std::string name = "lisi";
    std::string frameName = "";
    if (frames != nullptr) {
        frames->SetFrameFuncName(name);
        frameName = frames->GetFrameFuncName();
    }
    EXPECT_EQ(true, name == frameName) << "ProcessFrameTest017 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest017: end.";
}

/**
 * @tc.name: ProcessFrameTest018
 * @tc.desc: test get frame func name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest018: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    std::string name = "wangwu";
    frames->SetFrameFuncName(name);
    std::string frameName = frames->GetFrameFuncName();
    EXPECT_EQ(true, name == frameName) << "ProcessFrameTest018 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest018: end.";
}

/**
 * @tc.name: ProcessFrameTest019
 * @tc.desc: test get frame func name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest019: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    std::string name = "maliu";
    frames->SetFrameFuncName(name);
    std::string frameName = frames->GetFrameFuncName();
    EXPECT_EQ(true, name == frameName) << "ProcessFrameTest019 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest019: end.";
}

/**
 * @tc.name: ProcessFrameTest020
 * @tc.desc: test get frame func name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest020: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    std::string name = "liuqi";
    std::string frameName = " ";
    if (frames != nullptr) {
        frames->SetFrameFuncName(name);
        frameName = frames->GetFrameFuncName();
    }
    EXPECT_EQ(true, name == frameName) << "ProcessFrameTest020 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest020: end.";
}

/**
 * @tc.name: ProcessFrameTest021
 * @tc.desc: test get frame func name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessFramesTest, ProcessFrameTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessFrameTest021: start.";
    std::shared_ptr<DfxFrames> frames = std::make_shared<DfxFrames>();
    std::string name = "";
    frames->SetFrameFuncName(name);
    std::string frameName = frames->GetFrameFuncName();
    EXPECT_EQ(true, name == frameName) << "ProcessFrameTest021 Failed";
    GTEST_LOG_(INFO) << "ProcessFrameTest021: end.";
}
}
