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
#include <dlfcn.h>
#include <pthread.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxArk"

static const char ARK_LIB_NAME[] = "libark_jsruntime.so";

static void* g_handle = nullptr;
static pthread_mutex_t g_mutex;
static int (*g_getArkNativeFrameInfoFn)(int, uintptr_t*, uintptr_t*, uintptr_t*, JsFrame*, size_t&);
static int (*g_stepArkManagedNativeFrameFn)(int, uintptr_t*, uintptr_t*, uintptr_t*, char*, size_t);
static int (*g_getArkJsHeapCrashInfoFn)(int, uintptr_t *, uintptr_t *, int, char *, size_t);
}

int DfxArk::GetArkNativeFrameInfo(int pid, uintptr_t& pc, uintptr_t& fp, uintptr_t& sp, JsFrame* frames, size_t& size)
{
    if (g_getArkNativeFrameInfoFn != nullptr) {
        return g_getArkNativeFrameInfoFn(pid, &pc, &fp, &sp, frames, size);
    }

    pthread_mutex_lock(&g_mutex);
    if (g_getArkNativeFrameInfoFn != nullptr) {
        pthread_mutex_unlock(&g_mutex);
        return g_getArkNativeFrameInfoFn(pid, &pc, &fp, &sp, frames, size);
    }

    if (g_handle == nullptr) {
        g_handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
        if (g_handle == nullptr) {
            LOGU("Failed to load library(%s).", dlerror());
            pthread_mutex_unlock(&g_mutex);
            return -1;
        }
    }

    *(void**)(&g_getArkNativeFrameInfoFn) = dlsym(g_handle, "get_ark_native_frame_info");
    if (!g_getArkNativeFrameInfoFn) {
        LOGU("Failed to find symbol(%s).", dlerror());
        g_handle = nullptr;
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_mutex);
    return g_getArkNativeFrameInfoFn(pid, &pc, &fp, &sp, frames, size);
}

int DfxArk::StepArkManagedNativeFrame(int pid, uintptr_t& pc, uintptr_t& fp, uintptr_t& sp, char* buf, size_t bufSize)
{
    if (g_stepArkManagedNativeFrameFn != nullptr) {
        return g_stepArkManagedNativeFrameFn(pid, &pc, &fp, &sp, buf, bufSize);
    }

    pthread_mutex_lock(&g_mutex);
    if (g_stepArkManagedNativeFrameFn != nullptr) {
        pthread_mutex_unlock(&g_mutex);
        return g_stepArkManagedNativeFrameFn(pid, &pc, &fp, &sp, buf, bufSize);
    }

    if (g_handle == nullptr) {
        g_handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
        if (g_handle == nullptr) {
            LOGU("Failed to load library(%s).", dlerror());
            pthread_mutex_unlock(&g_mutex);
            return -1;
        }
    }

    *(void**)(&g_stepArkManagedNativeFrameFn) = dlsym(g_handle, "step_ark_managed_native_frame");
    if (!g_stepArkManagedNativeFrameFn) {
        LOGU("Failed to find symbol(%s).", dlerror());
        g_handle = nullptr;
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_mutex);
    return g_stepArkManagedNativeFrameFn(pid, &pc, &fp, &sp, buf, bufSize);
}

int DfxArk::GetArkJsHeapCrashInfo(int pid, uintptr_t& x20, uintptr_t& fp, int outJsInfo, char* buf, size_t bufSize)
{
    if (g_getArkJsHeapCrashInfoFn != nullptr) {
        return g_getArkJsHeapCrashInfoFn(pid, &x20, &fp, outJsInfo, buf, bufSize);
    }

    pthread_mutex_lock(&g_mutex);
    if (g_getArkJsHeapCrashInfoFn != nullptr) {
        pthread_mutex_unlock(&g_mutex);
        return g_getArkJsHeapCrashInfoFn(pid, &x20, &fp, outJsInfo, buf, bufSize);
    }

    if (g_handle == nullptr) {
        g_handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
        if (g_handle == nullptr) {
            LOGU("Failed to load library(%s).", dlerror());
            pthread_mutex_unlock(&g_mutex);
            return -1;
        }
    }

    *(void**)(&g_getArkJsHeapCrashInfoFn) = dlsym(g_handle, "get_ark_js_heap_crash_info");
    if (!g_getArkJsHeapCrashInfoFn) {
        LOGU("Failed to find symbol(%s).", dlerror());
        g_handle = nullptr;
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_mutex);
    return g_getArkJsHeapCrashInfoFn(pid, &x20, &fp, outJsInfo, buf, bufSize);
}
} // namespace HiviewDFX
} // namespace OHOS
