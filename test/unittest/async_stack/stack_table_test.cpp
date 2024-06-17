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

class StackTableTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

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
}
} // namespace HiviewDFX
} // namespace OHOS