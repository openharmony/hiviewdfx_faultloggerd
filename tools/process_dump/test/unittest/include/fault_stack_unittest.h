/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

/* This files contains unit test header of process dump module. */

#ifndef FAULT_STACK_UNITTEST_H
#define FAULT_STACK_UNITTEST_H

#include <gtest/gtest.h>

namespace OHOS {
namespace HiviewDFX {
class FaultStackUnittest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULT_STACK_UNITTEST_H
