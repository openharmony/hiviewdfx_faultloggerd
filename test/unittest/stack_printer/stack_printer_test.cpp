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
#include <vector>

#include "stack_printer.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#if defined(__aarch64__)
static std::vector<std::vector<uintptr_t>> pcsVec = {
    {
        0x0000007f0b0da13c,
        0x0000007f19709a99,
        0x0000007f1ac8e1b4,
        0x0000007f1af60370,
        0x0000007f14ddbf9c,
        0x0000007f147adc54,
        0x0000007f1479c8e0,
        0x0000007f14845870,
        0x0000007f12a2d55c,
        0x0000007f12ae6590,
        0x0000007f12af1df8,
        0x0000007f12adfbe0,
        0x0000007f12af1150,
        0x0000007f12ae8e4c,
        0x0000007f12af0284,
        0x0000007f11e9e8f0,
        0x0000007f11e9c374,
        0x0000007f12074ab8,
        0x0000007f1457c4e0,
        0x0000007f1457bcc8,
        0x0000007f145a4818,
        0x0000007f14606d64,
        0x0000007f18f65d34,
        0x0000007f18f4f5f4,
        0x0000007f18f778dc,
        0x0000007f18efa87c,
        0x0000007f18efacd0,
        0x0000007f1f7df700,
        0x0000007f1f7c5c9c
    },
    {
        0x0000007f1ad84888,
        0x0000007f0b0da140,
        0x0000007f0ab16de4,
        0x0000007f19709a99,
        0x0000007f1ac8e1b4,
        0x0000007f1af60370,
        0x0000007f14ddbf9c,
        0x0000007f147adc54,
        0x0000007f1479c8e0,
        0x0000007f14845870,
        0x0000007f12a2d55c,
        0x0000007f12ae6590,
        0x0000007f12af1df8,
        0x0000007f12adfbe0,
        0x0000007f12af1150,
        0x0000007f12ae8e4c,
        0x0000007f12af0284,
        0x0000007f11e9e8f0,
        0x0000007f11e9c374,
        0x0000007f12074ab8,
        0x0000007f1457c4e0,
        0x0000007f1457bcc8,
        0x0000007f145a4818,
        0x0000007f14606d64,
        0x0000007f18f65d34,
        0x0000007f18f4f5f4,
        0x0000007f18f778dc,
        0x0000007f18efa87c,
        0x0000007f18efacd0,
        0x0000007f1f7df700,
        0x0000007f1f7c5c9c
    },
    {
        0x0000007f0ab2c670,
        0x0000007f19709a99,
        0x0000007f1ac8e1b4,
        0x0000007f1af60370,
        0x0000007f14ddbf9c,
        0x0000007f147adc54,
        0x0000007f1479c8e0,
        0x0000007f14845870,
        0x0000007f12a2d55c,
        0x0000007f12ae6590,
        0x0000007f12af1df8,
        0x0000007f12adfbe0,
        0x0000007f12af1150,
        0x0000007f12ae8e4c,
        0x0000007f12af0284,
        0x0000007f11e9e8f0,
        0x0000007f11e9c374,
        0x0000007f12074ab8,
        0x0000007f1457c4e0,
        0x0000007f1457bcc8,
        0x0000007f145a4818,
        0x0000007f14606d64,
        0x0000007f18f65d34,
        0x0000007f18f4f5f4,
        0x0000007f18f778dc,
        0x0000007f18efa87c,
        0x0000007f18efacd0,
        0x0000007f1f7df700,
        0x0000007f1f7c5c9c
    },
    {
        0x0000007f0ab2c60c,
        0x0000007f19709a99,
        0x0000007f1ac8e1b4,
        0x0000007f1af60370,
        0x0000007f14ddbf9c,
        0x0000007f147adc54,
        0x0000007f1479c8e0,
        0x0000007f14845870,
        0x0000007f12a2d55c,
        0x0000007f12ae6590,
        0x0000007f12af1df8,
        0x0000007f12adfbe0,
        0x0000007f12af1150,
        0x0000007f12ae8e4c,
        0x0000007f12af0284,
        0x0000007f11e9e8f0,
        0x0000007f11e9c374,
        0x0000007f12074ab8,
        0x0000007f1457c4e0,
        0x0000007f1457bcc8,
        0x0000007f145a4818,
        0x0000007f14606d64,
        0x0000007f18f65d34,
        0x0000007f18f4f5f4,
        0x0000007f18f778dc,
        0x0000007f18efa87c,
        0x0000007f18efacd0,
        0x0000007f1f7df700,
        0x0000007f1f7c5c9c
    },
    {
        0x0000007fa409ac8c,
        0x0000007f2291931c,
        0x0000007f22920ddc,
        0x0000007f2292453c,
        0x0000007f2292f17c,
        0x0000007f22932a44,
        0x0000007f18b22f58,
        0x0000007f17b057c4,
        0x000000556e13254c,
        0x000000556e13c5a0,
        0x000000556e13a3dc,
        0x0000007fa38d6d68,
        0x0000007fa38d687c,
        0x0000007fa38d3fb4,
        0x0000007fa38d3b30,
        0x000000556e1380e0,
        0x000000556e135ac0,
        0x0000007fa4070618,
        0x000000556e132234
    },
    {
        0x0000007fa409ac8c,
        0x0000007f2291931c,
        0x0000007f22920ddc,
        0x0000007f2292453c,
        0x0000007f2292f17c,
        0x0000007f22932a44,
        0x0000007f18b22f58,
        0x0000007f17b057c4,
        0x000000556e13254c,
        0x000000556e13c5a0,
        0x000000556e13a3dc,
        0x0000007fa38d6d68,
        0x0000007fa38d687c,
        0x0000007fa38d3fb4,
        0x0000007fa38d3b30,
        0x000000556e1380e0,
        0x000000556e135ac0,
        0x0000007fa4070618,
        0x000000556e132234
    },
    {
        0x0000007fa409ac8c,
        0x0000007f2291931c,
        0x0000007f22920ddc,
        0x0000007f2292453c,
        0x0000007f2292f17c,
        0x0000007f22932a44,
        0x0000007f18b22f58,
        0x0000007f17b057c4,
        0x000000556e13254c,
        0x000000556e13c5a0,
        0x000000556e13a3dc,
        0x0000007fa38d6d68,
        0x0000007fa38d687c,
        0x0000007fa38d3fb4,
        0x0000007fa38d3b30,
        0x000000556e1380e0,
        0x000000556e135ac0,
        0x0000007fa4070618,
        0x000000556e132234
    },
    {
        0x0000007fa409ac8c,
        0x0000007f2291931c,
        0x0000007f22920ddc,
        0x0000007f2292453c,
        0x0000007f2292f17c,
        0x0000007f22932a44,
        0x0000007f18b22f58,
        0x0000007f17b057c4,
        0x000000556e13254c,
        0x000000556e13c5a0,
        0x000000556e13a3dc,
        0x0000007fa38d6d68,
        0x0000007fa38d687c,
        0x0000007fa38d3fb4,
        0x0000007fa38d3b30,
        0x000000556e1380e0,
        0x000000556e135ac0,
        0x0000007fa4070618,
        0x000000556e132234
    },
    {
        0x0000007fa409ac8c,
        0x0000007f2291931c,
        0x0000007f22920ddc,
        0x0000007f2292453c,
        0x0000007f2292f17c,
        0x0000007f22932a44,
        0x0000007f18b22f58,
        0x0000007f17b057c4,
        0x000000556e13254c,
        0x000000556e13c5a0,
        0x000000556e13a3dc,
        0x0000007fa38d6d68,
        0x0000007fa38d687c,
        0x0000007fa38d3fb4,
        0x0000007fa38d3b30,
        0x000000556e1380e0,
        0x000000556e135ac0,
        0x0000007fa4070618,
        0x000000556e132234
    },
    {
        0x0000007fa409ac8c,
        0x0000007f2291931c,
        0x0000007f22920ddc,
        0x0000007f2292453c,
        0x0000007f2292f17c,
        0x0000007f22932a44,
        0x0000007f18b22f58,
        0x0000007f17b057c4,
        0x000000556e13254c,
        0x000000556e13c5a0,
        0x000000556e13a3dc,
        0x0000007fa38d6d68,
        0x0000007fa38d687c,
        0x0000007fa38d3fb4,
        0x0000007fa38d3b30,
        0x000000556e1380e0,
        0x000000556e135ac0,
        0x0000007fa4070618,
        0x000000556e132234
    }
};
const std::string MAPS_PATH = "/data/test/resource/testdata/stack_printer_testmaps64";
#elif defined(__arm__)
static std::vector<std::vector<uintptr_t>> pcsVec = {
    {
        0xedc80a80,
        0xed87fe33,
        0xed87f8f9,
        0xeda22db5,
        0xeda0ff2b,
        0xedaaf9cf,
        0xedbef469,
        0xe9934d59,
        0xe953e4fd,
        0xe9533a47,
        0xe959dedb,
        0xe7feae75,
        0xe806f8f9,
        0xe806f773,
        0xe807822f,
        0xe806b24d,
        0xe8077809,
        0xe8070aeb,
        0xe80713d1,
        0xe8076de5,
        0xe8076c6b,
        0xe8076c1d,
        0xe77bbc87,
        0xe77ba81f,
        0xe77ba0af,
        0xe78e385f,
        0xe93abc63,
        0xe93ab6d9,
        0xe93c866d,
        0xe93c7b23,
        0xe940e30b,
        0xed48553f,
        0xed4793e9,
        0xed490881,
        0xed440609,
        0xed440945,
        0xed590c0b,
        0xed57eca3,
        0xed57dbc1,
        0xee73601b
    },
    {
        0xeda18e0e,
        0xeda0ff2b,
        0xedaaf9cf,
        0xedbef469,
        0xe9934d59,
        0xe953e4fd,
        0xe9533a47,
        0xe959dedb,
        0xe7feae75,
        0xe806f8f9,
        0xe806f773,
        0xe807822f,
        0xe806b24d,
        0xe8077809,
        0xe8070aeb,
        0xe80713d1,
        0xe8076de5,
        0xe8076c6b,
        0xe8076c1d,
        0xe77bbc87,
        0xe77ba81f,
        0xe77ba0af,
        0xe78e385f,
        0xe93abc63,
        0xe93ab6d9,
        0xe93c866d,
        0xe93c7b23,
        0xe940e30b,
        0xed48553f,
        0xed4793e9,
        0xed490881,
        0xed440609,
        0xed440945,
        0xed590c0b,
        0xed57eca3,
        0xed57dbc1,
        0xee73601b
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    },
    {
        0xf7a16458,
        0xf5793273,
        0xf5798c65,
        0xf579b403,
        0xf57a2ea9,
        0xf57a59af,
        0xf0b75909,
        0xf4984e89,
        0x00a410fd,
        0x00a4897d,
        0x00a46f05,
        0xf710f8a3,
        0xf710f511,
        0xf710d713,
        0xf710d3b5,
        0x00a454a7,
        0x00a437f7,
        0xf79e2fe4,
        0x00a40f18,
        0x00a40ebc
    }
};
const std::string MAPS_PATH = "/data/test/resource/testdata/stack_printer_testmaps32";
#endif

static std::vector<uint64_t> timestamps = {
    1719392400123456789,
    1719392400123606789,
    1719392400123756789,
    1719392400123906789,
    1719392400124056789,
    1719392400124206789,
    1719392400124356789,
    1719392400124506789,
    1719392400124656789,
    1719392400124706789,
};

class StackPrinterTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void StackPrinterTest::SetUpTestCase(void)
{
    GTEST_LOG_(INFO) << "SetUpTestCase.";
}

void StackPrinterTest::TearDownTestCase(void)
{
    GTEST_LOG_(INFO) << "TearDownTestCase.";
}

void StackPrinterTest::SetUp(void)
{
    GTEST_LOG_(INFO) << "SetUp.";
}

void StackPrinterTest::TearDown(void)
{
    GTEST_LOG_(INFO) << "TearDown.";
}

/**
 * @tc.name: StackPrinterTest001
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_001: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    bool flag = stackPrinter->InitUniqueTable(getpid(), 0);
    ASSERT_FALSE(flag);
    uint32_t uniqueStableSize = 128 * 1024;
    flag = stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    ASSERT_TRUE(flag);
    GTEST_LOG_(INFO) << "StackPrinterTest_001: end.";
}

/**
 * @tc.name: StackPrinterTest002
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_002: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    bool flag = stackPrinter->PutPcsInTable(pcsVec[0], gettid(), timestamps[0]);
    ASSERT_FALSE(flag);

    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        flag = stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
        ASSERT_TRUE(flag);
    }
    GTEST_LOG_(INFO) << "StackPrinterTest_002: end.";
}

/**
 * @tc.name: StackPrinterTest003
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_003: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    std::vector<TimeStampedPcs> timeStampedPcsVec;
    for (size_t i = 0; i < pcsVec.size(); i++) {
        TimeStampedPcs p = {
            .snapshotTime = timestamps[i],
            .pcVec = pcsVec[i]
        };
        timeStampedPcsVec.emplace_back(p);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);
    std::string stack = stackPrinter->GetFullStack(timeStampedPcsVec);
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";
    GTEST_LOG_(INFO) << "StackPrinterTest_003: end.\n";
}

/**
 * @tc.name: StackPrinterTest004
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_004: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);
    std::string stack = stackPrinter->GetTreeStack(gettid());
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";
    GTEST_LOG_(INFO) << "\n================================================\n";

    stack = stackPrinter->GetTreeStack(gettid(), true);
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";
    GTEST_LOG_(INFO) << "\n================================================\n";

    stack = stackPrinter->GetTreeStack(gettid(), true, timestamps[2], timestamps[6]);
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";
    GTEST_LOG_(INFO) << "StackPrinterTest_004: end.";
}

/**
 * @tc.name: StackPrinterTest005
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_005: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);

    uint64_t startTime = timestamps[0] - 10;
    uint64_t endTime = timestamps[0] - 1;
    // shoud return empty stack
    std::string emptyStack = stackPrinter->GetHeaviestStack(gettid(), startTime, endTime);
    ASSERT_EQ(emptyStack, "");
    std::string stack = stackPrinter->GetHeaviestStack(gettid());
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";
    GTEST_LOG_(INFO) << "\n================================================\n";

    stack = stackPrinter->GetHeaviestStack(gettid(), timestamps[2], timestamps[6]);
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";
    GTEST_LOG_(INFO) << "StackPrinterTest_005: end.";
}

/**
 * @tc.name: StackPrinterTest006
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_006: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);
    std::map<int, std::vector<SampledFrame>> threadSampledFrames = stackPrinter->GetThreadSampledFrames();
    ASSERT_FALSE(threadSampledFrames.empty());
    std::vector<SampledFrame> frames = threadSampledFrames[getpid()];
    ASSERT_FALSE(frames.empty());

    const int indent = 2;
    for (const auto& frame : frames) {
        ASSERT_EQ(frame.indent, indent * frame.level);
    }
    GTEST_LOG_(INFO) << "StackPrinterTest_006: end.";
}

/**
 * @tc.name: StackPrinterTest007
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_007: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);
    std::map<int, std::vector<SampledFrame>> threadSampledFrames = stackPrinter->GetThreadSampledFrames();
    ASSERT_FALSE(threadSampledFrames.empty());

    std::stringstream ss;
    StackPrinter::SerializeSampledFrameMap(threadSampledFrames, ss);
    GTEST_LOG_(INFO) << "serialized map: " << ss.str().c_str();

    std::map<int, std::vector<SampledFrame>> deserializeMap = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_EQ(threadSampledFrames.size(), deserializeMap.size());
    auto it1 = threadSampledFrames.cbegin();
    auto it2 = deserializeMap.cbegin();
    while (it1 != threadSampledFrames.cend() && it2 != deserializeMap.cend()) {
        ASSERT_EQ(it1->first, it2->first);
        ASSERT_EQ(it1->second.size(), it2->second.size());
        for (size_t i = 0; i < it1->second.size(); i++) {
            ASSERT_EQ(it1->second[i].indent, it2->second[i].indent);
            ASSERT_EQ(it1->second[i].count, it2->second[i].count);
            ASSERT_EQ(it1->second[i].level, it2->second[i].level);
            ASSERT_EQ(it1->second[i].pc, it2->second[i].pc);
            ASSERT_EQ(it1->second[i].isLeaf, it2->second[i].isLeaf);
            ASSERT_EQ(it1->second[i].timestamps, it2->second[i].timestamps);
        }
        it1++;
        it2++;
    }
    GTEST_LOG_(INFO) << "StackPrinterTest_007: end.";
}

/**
 * @tc.name: StackPrinterTest008
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_008: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);
    std::map<int, std::vector<SampledFrame>> threadSampledFrames = stackPrinter->GetThreadSampledFrames();
    ASSERT_FALSE(threadSampledFrames.empty());

    std::vector<SampledFrame> sampledFrameVec = threadSampledFrames[gettid()];
    ASSERT_FALSE(sampledFrameVec.empty());

    std::string stack = StackPrinter::PrintTreeStackBySampledStack(sampledFrameVec, true, unwinder, maps);
    ASSERT_NE(stack, "");
    GTEST_LOG_(INFO) << "stack:\n" << stack.c_str() << "\n";

    GTEST_LOG_(INFO) << "StackPrinterTest_008: end.";
}

/**
 * @tc.name: StackPrinterTest009
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_009: start.";
    std::unique_ptr<StackPrinter> stackPrinter = std::make_unique<StackPrinter>();
    uint32_t uniqueStableSize = 128 * 1024;
    stackPrinter->InitUniqueTable(getpid(), uniqueStableSize);
    for (size_t i = 0; i < pcsVec.size(); i++) {
        stackPrinter->PutPcsInTable(pcsVec[i], gettid(), timestamps[i]);
    }

    std::shared_ptr<Unwinder> unwinder = std::make_shared<Unwinder>(false);
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(getpid(), MAPS_PATH);
    stackPrinter->SetUnwindInfo(unwinder, maps);
    std::map<int, std::vector<SampledFrame>> threadSampledFrames = stackPrinter->GetThreadSampledFrames();
    ASSERT_FALSE(threadSampledFrames.empty());

    std::vector<SampledFrame> sampledFrameVec = threadSampledFrames[gettid()];
    ASSERT_FALSE(sampledFrameVec.empty());

    for (const auto& originFrame : sampledFrameVec) {
        std::stringstream ss;
        ss << originFrame;
        std::string frameStr = std::to_string(originFrame.indent) + " " + std::to_string(originFrame.count) + " " +
                               std::to_string(originFrame.level) + " " + std::to_string(originFrame.pc) + " " +
                               std::to_string(originFrame.isLeaf) + " " + std::to_string(originFrame.timestamps.size());
        for (const auto& timestamp : originFrame.timestamps) {
            frameStr = frameStr + " " + std::to_string(timestamp);
        }
        ASSERT_EQ(ss.str(), frameStr);

        SampledFrame sampledFrame;
        ss >> sampledFrame;

        ASSERT_EQ(originFrame.indent, sampledFrame.indent);
        ASSERT_EQ(originFrame.count, sampledFrame.count);
        ASSERT_EQ(originFrame.level, sampledFrame.level);
        ASSERT_EQ(originFrame.pc, sampledFrame.pc);
        ASSERT_EQ(originFrame.isLeaf, sampledFrame.isLeaf);
        ASSERT_EQ(originFrame.timestamps, sampledFrame.timestamps);
    }

    GTEST_LOG_(INFO) << "StackPrinterTest_009: end.";
}

/**
 * @tc.name: StackPrinterTest010
 * @tc.desc: test StackPrinter functions
 * @tc.type: FUNC
 */
HWTEST_F(StackPrinterTest, StackPrinterTest_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackPrinterTest_010: start.";
    // SampledFrame string to be deserialized: mapSize + \n + tid + vecSize + frameinfo + \n
    // frameinfo: indent + count + level + pc + isLeaf + timestamps.size() + timestamps(set empty)
    std::stringstream ss("11\n" + std::to_string(gettid()) + " 1 1 5 3 9630076 0 0\n");
    auto result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_FALSE(ss.fail());
    ASSERT_TRUE(result.empty());

    ss.str("");
    ss.clear();
    ss.str("invalidMapSize");
    result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_TRUE(ss.fail());
    ASSERT_TRUE(result.empty());

    ss.str("");
    ss.clear();
    ss.str("1\n-1 1 1 5 3 9630076 0 0\n");
    result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_FALSE(ss.fail());
    ASSERT_TRUE(result.empty());

    ss.str("");
    ss.clear();
    ss.str("1\n65537 1 1 5 3 9630076 0 0\n");
    result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_FALSE(ss.fail());
    ASSERT_TRUE(result.empty());

    ss.str("");
    ss.clear();
    ss.str("1\ninvalidTid");
    result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_TRUE(ss.fail());
    ASSERT_TRUE(result.empty());

    ss.str("");
    ss.clear();
    ss.str("1\ninvalidTid 1");
    result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_TRUE(ss.fail());
    ASSERT_TRUE(result.empty());

    ss.str("");
    ss.clear();
    ss.str("1\n" + std::to_string(gettid()) + " 1 1 5 3 9630076 0 0\n");
    result = StackPrinter::DeserializeSampledFrameMap(ss);
    ASSERT_FALSE(ss.fail());
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[gettid()].size(), 1);

    GTEST_LOG_(INFO) << "StackPrinterTest_010: end.";
}
}  // namespace HiviewDFX
}  // namespace OHOS