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

#include "dfx_ark.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxArk"
}

void* DfxArk::handle_ = nullptr;
std::mutex DfxArk::mutex_;
//static int (*StepArkManagedNativeFrameFn)(int, uintptr_t*, uintptr_t*, uintptr_t*, char*, size_t);
//static int (*GetArkJsHeapCrashInfoFn)(int, uintptr_t *, uintptr_t *, int, char *, size_t);

int DfxArk::StepArkManagedNativeFrame(int pid, uintptr_t* pc, uintptr_t* fp, uintptr_t* sp, char* buf, size_t bufSize)
{
    return 0;
}

int DfxArk::GetArkJsHeapCrashInfo(int pid, uintptr_t* x20, uintptr_t* fp, int outJsInfo, char* buf, size_t bufSize)
{
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
