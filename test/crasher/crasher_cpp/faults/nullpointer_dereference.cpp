/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "nullpointer_dereference.h"

#include <cstdio>
#include <memory>
#include <vector>

using namespace OHOS::HiviewDFX;

int TestNullPointerDereferenceCrash0()
{
    std::shared_ptr<NullPointerDereference> testcase = nullptr;
    testcase->Print(); // may not crash
    testcase->PrintMemberValue(); // crash here
    return 0;
}

int TestNullPointerDereferenceCrash1()
{
    auto testcase = std::make_shared<NullPointerDereference>();
    testcase.reset();
    testcase->Print(); // may not crash
    return 0;
}

int TestNullPointerDereferenceCrash2()
{
    auto testcase = std::make_shared<NullPointerDereference>();
    testcase.reset();
    testcase->PrintMemberValue(); // crash here
    return 0;
}

int TestNullPointerDereferenceCrash3()
{
    std::vector<std::weak_ptr<NullPointerDereference>> refs;
    auto ref = refs.front(); // crash here
    if (auto sharedRef = ref.lock()) {
        printf("TestNullPointerDereferenceCrash3:%p:\n", sharedRef.get());
    }
    return 0;
}

namespace OHOS {
namespace HiviewDFX {
void NullPointerDereference::Print() const
{
    printf("NullPointerDereference::Print\n");
}

void NullPointerDereference::PrintMemberValue() const
{
    printf("NullPointerDereference::PrintMemberValue:\n");
    printf("%d:\n", intTypeValue_);
    printf("%d:\n", intTypeValue0_);
}
} // namespace HiviewDFX
} // namespace OHOS
