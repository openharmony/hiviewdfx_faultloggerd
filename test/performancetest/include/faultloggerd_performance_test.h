/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef FAULT_PERFORMANCE_TEST_H
#define FAULT_PERFORMANCE_TEST_H

#include <gtest/gtest.h>

namespace OHOS {
namespace HiviewDFX {
class FaultPerformanceTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    static std::string ForkAndRootCommands(const std::vector<std::string>& cmds);
    static void StartRootCrasherLoop();
    static void KillCrasherLoopForSomeCase();
    static int looprootPid;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULT_PERFORMANCE_TEST_H
