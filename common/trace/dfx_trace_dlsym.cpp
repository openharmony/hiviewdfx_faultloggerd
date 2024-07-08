/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#if DFX_ENABLE_TRACE && is_ohos && !is_mingw
#include "dfx_trace_dlsym.h"
#include <cstdarg>
#include <dlfcn.h>
#include <securec.h>
#include <mutex>
#endif

namespace OHOS {
namespace HiviewDFX {
static bool g_enableTrace = false;
void DfxEnableTraceDlsym(bool enableTrace)
{
    g_enableTrace = enableTrace;
}
#if DFX_ENABLE_TRACE && is_ohos && !is_mingw
namespace {
#ifndef TAG_APP
#define TAG_APP (1ULL << 62)
#endif
typedef void (*StartTraceFunc)(uint64_t tag, const char* name);
typedef void (*FinishTraceFunc)(uint64_t tag);
[[maybe_unused]]static StartTraceFunc g_startTrace = nullptr;
[[maybe_unused]]static FinishTraceFunc g_finishTrace = nullptr;
}
void DfxStartTraceDlsym(const char* name)
{
    if (!g_enableTrace) {
        return;
    }
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        const char* startTraceFuncName = "StartTraceCwrapper";
        g_startTrace = reinterpret_cast<StartTraceFunc>(dlsym(RTLD_DEFAULT, startTraceFuncName));
    });
    if (g_startTrace != nullptr) {
        g_startTrace(TAG_APP, name);
    }
}
void DfxStartTraceDlsymFormat(const char *fmt, ...)
{
    if (!g_enableTrace) {
        return;
    }
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        const char* startTraceFuncName = "StartTraceCwrapper";
        g_startTrace = reinterpret_cast<StartTraceFunc>(dlsym(RTLD_DEFAULT, startTraceFuncName));
    });
    if (g_startTrace == nullptr) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    char traceName[TRACE_BUF_LEN] = {0};
    int ret = vsnprintf_s(traceName, sizeof(traceName), sizeof(traceName) - 1, fmt, args);
    va_end(args);
    if (ret == -1) {
        return;
    }
    g_startTrace(TAG_APP, traceName);
}

void DfxFinishTraceDlsym(void)
{
    if (!g_enableTrace) {
        return;
    }
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        const char* finishTraceFuncName = "FinishTraceCwrapper";
        g_finishTrace = reinterpret_cast<FinishTraceFunc>(dlsym(RTLD_DEFAULT, finishTraceFuncName));
    });
    if (g_finishTrace != nullptr) {
        g_finishTrace(TAG_APP);
    }
}

void FormatTraceName(char *name, size_t size, const char *fmt, ...)
{
    if (!g_enableTrace) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf_s(name, size, size - 1, fmt, args);
    va_end(args);
    if (ret == -1) {
        strcpy_s(name, size, "DefaultTraceName");
    }
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
