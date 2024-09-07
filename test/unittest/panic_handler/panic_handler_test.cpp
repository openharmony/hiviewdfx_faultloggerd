/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <cstring>
#include <gtest/gtest.h>
#include <unistd.h>
#include <vector>
#ifdef HISYSEVENT_ENABLE
#include "hisysevent_manager.h"
#include "rustpanic_listener.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace {
static std::shared_ptr<RustPanicListener> panicListener = nullptr;
}
class PanicHandlerTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void PanicHandlerTest::SetUpTestCase()
{}

void PanicHandlerTest::TearDownTestCase()
{}

void PanicHandlerTest::SetUp()
{
    chmod("/data/rustpanic_maker", 0755); // 0755 : -rwxr-xr-x
}

void PanicHandlerTest::TearDown()
{
    panicListener = nullptr;
}

static void ForkAndTriggerRustPanic(bool ismain)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "ForkAndTriggerRustPanic: Failed to fork panic process.";
        return;
    } else if (pid == 0) {
        if (ismain) {
            execl("/data/rustpanic_maker", "rustpanic_maker", "main", nullptr);
        } else {
            execl("/data/rustpanic_maker", "rustpanic_maker", "child", nullptr);
        }
    }
}

static void ListenAndCheckHiSysevent()
{
    panicListener = std::make_shared<RustPanicListener>();
    ListenerRule tagRule("RELIABILITY", "RUST_PANIC", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(panicListener, sysRules);
}

/**
 * @tc.name: PanicHandlerTest001
 * @tc.desc: test panic in main thread
 * @tc.type: FUNC
 * @tc.require: issueI6HM7C
 */
HWTEST_F(PanicHandlerTest, PanicHandlerTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "PanicHandlerTest001: start.";
    ListenAndCheckHiSysevent();
    panicListener->SetKeyWord("panic in main thread");
    ForkAndTriggerRustPanic(true);
    if (panicListener != nullptr) {
        GTEST_LOG_(INFO) << "PanicHandlerTest001: ready to check hisysevent reason keyword.";
        ASSERT_TRUE(panicListener->CheckKeywordInReasons());
    }
    GTEST_LOG_(INFO) << "PanicHandlerTest001: end.";
}

/**
 * @tc.name: PanicHandlerTest002
 * @tc.desc: test panic in child thread
 * @tc.type: FUNC
 * @tc.require: issueI6HM7C
 */
HWTEST_F(PanicHandlerTest, PanicHandlerTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "PanicHandlerTest002: start.";
    ListenAndCheckHiSysevent();
    panicListener->SetKeyWord("panic in child thread");
    ForkAndTriggerRustPanic(false);
    if (panicListener != nullptr) {
        GTEST_LOG_(INFO) << "PanicHandlerTest002: ready to check hisysevent reason keyword.";
        ASSERT_TRUE(panicListener->CheckKeywordInReasons());
    }
    GTEST_LOG_(INFO) << "PanicHandlerTest002: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
#endif