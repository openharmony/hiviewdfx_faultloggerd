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

/* This files is unit test of process dump thread module. */

#ifndef MOCK_DFX_THREAD_H
#define MOCK_DFX_THREAD_H

#include <gmock/gmock.h>

namespace OHOS {
namespace HiviewDFX {
class MockDfxThread {
public:
    MockDfxThread() = default;
    virtual ~MockDfxThread()  = default;
    virtual pid_t GetThreadId() = 0;
    virtual void PrintThread(const int32_t fd, bool isSignalDump) = 0;
};

class DfxThread : public MockDfxThread {
public:
    DfxThread() = default;
    virtual ~DfxThread()  = default;
    MOCK_METHOD0(GetThreadId, pid_t());
    MOCK_METHOD2(PrintThread, void(const int32_t fd, bool isSignalDump));
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // MOCK_DFX_THREAD_H
