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

/* This files contains DfxProcess unit test functions */


#ifndef MOCK_DFX_THREAD_H
#define MOCK_DFX_THREAD_H

#include <gmock/gmock.h>
#include "dfx_process.h"

namespace OHOS {
namespace HiviewDFX {
class MockDfxProcess : public DfxProcess {
public:
    MockDfxProcess() = default;
    virtual ~MockDfxProcess() = default;
    MOCK_METHOD1(PrintProcess, void(int32_t fd));
    MOCK_METHOD2(PrintProcessWithSiginfo, void(const std::shared_ptr<siginfo_t> info, int32_t fd));
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // MOCK_DFX_THREAD_H
