/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include <sys/types.h>

#include "unique_stack_table.h"

using namespace OHOS::HiviewDFX;
using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace {
static const int BUF_LEN = 100;
}

class StackTableTest : public testing::Test {};

namespace {
/**
 * @tc.name: StackTableTest001
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest001: start.";
    Node node;
    node.value = 1;
    char buf[BUF_LEN] = "6fd1bf000-6fd2bf000 rw-p 00000000 b3:07 1882 [anon:async_stack_table]";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(buf, sizeof(buf));
    ASSERT_EQ(stackTable->ImportNode(0, node), true);
    ASSERT_EQ(stackTable->GetHeadNode()->value, node.value);
    size_t functionsSize = stackTable->GetWriteSize();
    uint32_t oldSize = stackTable->GetTabelSize();
    uint32_t usedNodes = stackTable->GetUsedIndexes().size();
    auto pid = stackTable->GetPid();
    size_t resultSize = sizeof(pid) + sizeof(stackTable->GetTabelSize());
    size_t usedNodesSize = sizeof(usedNodes) + usedNodes * sizeof(uint32_t) + usedNodes * sizeof(uint64_t);
    ASSERT_EQ(usedNodesSize + resultSize, functionsSize);
    ASSERT_EQ(stackTable->Resize(), true);
    ASSERT_NE(stackTable->GetTabelSize(), oldSize);
    GTEST_LOG_(INFO) << "StackTableTest001: end.";
}

/**
 * @tc.name: StackTableTest002
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest002: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(nullptr,
                                                                                 MAX_NODES_CNT * MAX_NODES_CNT);
    bool ret = stackTable->Init();
    ASSERT_EQ(ret, false);
    ret = stackTable->Resize();
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "StackTableTest002: end.";
}

/**
 * @tc.name: StackTableTest003
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest003: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(nullptr, 0);
    bool ret = stackTable->Init();
    ASSERT_EQ(ret, false);
    ret = stackTable->Resize();
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "StackTableTest003: end.";
}

/**
 * @tc.name: StackTableTest004
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest004: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(nullptr,
                                                                                 MAX_NODES_CNT * MAX_NODES_CNT);
    uint64_t ret = stackTable->PutPcsInTable(nullptr, nullptr, 0);
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "StackTableTest004: end.";
}

/**
 * @tc.name: StackTableTest005
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest005: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(nullptr,
                                                                                 MAX_NODES_CNT * MAX_NODES_CNT);
    size_t ret = stackTable->GetWriteSize();
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "StackTableTest005: end.";
}

/**
 * @tc.name: StackTableTest006
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest006: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(nullptr, 0);
    StackId stackId;
    std::vector<uintptr_t> pcs;
    bool ret = stackTable->GetPcsByStackId(stackId, pcs);
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "StackTableTest006: end.";
}

/**
 * @tc.name: StackTableTest007
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest007: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(0, 0);
    Node node;
    bool ret = stackTable->ImportNode(MAX_NODES_CNT, node);
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "StackTableTest007: end.";
}

/**
 * @tc.name: StackTableTest008
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest008: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(0, 0);
    uint64_t ret = stackTable->PutPcInSlot(0, 0);
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "StackTableTest008: end.";
}

/**
 * @tc.name: StackTableTest009
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest009: start.";
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(0, 0);
    Node* ret = stackTable->GetFrame(MAX_NODES_CNT);
    ASSERT_EQ(ret, nullptr);
    GTEST_LOG_(INFO) << "StackTableTest009: end.";
}


/**
 * @tc.name: StackTableTest010
 * @tc.desc: test StackTable functions
 * @tc.type: FUNC
 */
HWTEST_F(StackTableTest, StackTableTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StackTableTest010: start.";
    const size_t size = (MAX_NODES_CNT + 10) * sizeof(Node);
    std::shared_ptr<UniqueStackTable> stackTable = make_shared<UniqueStackTable>(nullptr,
                                                                                 size);
    bool ret = stackTable->Init();
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "StackTableTest010: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS