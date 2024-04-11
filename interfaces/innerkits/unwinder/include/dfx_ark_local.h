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
#ifndef DFX_ARK_LOCAL_H
#define DFX_ARK_LOCAL_H

#include <atomic>
#include <cinttypes>
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

namespace OHOS {
namespace HiviewDFX {
namespace {
#if defined(ENABLE_MIXSTACK)
typedef bool (*ReadMemFunc)(void *, uintptr_t, uintptr_t *);
int (*g_stepArkFn)(void *ctx, OHOS::HiviewDFX::ReadMemFunc readMemFn,
    uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, bool *isJsFrame);
pthread_mutex_t g_mutex;
#endif
}

class DfxArkLocal {
public:
#if defined(ENABLE_MIXSTACK)
    static int StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
        uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, bool *isJsFrame)
    {
        if (g_stepArkFn != nullptr) {
            return g_stepArkFn(obj, readMemFn, fp, sp, pc, isJsFrame);
        }

        const char* arkFuncName = "step_ark";
        pthread_mutex_lock(&g_mutex);
        *(void**)(&(g_stepArkFn)) = dlsym(RTLD_DEFAULT, arkFuncName);
        if ((g_stepArkFn) == NULL) {
            return -1;
        }
        pthread_mutex_unlock(&g_mutex);
        if (g_stepArkFn != nullptr) {
            return g_stepArkFn(obj, readMemFn, fp, sp, pc, isJsFrame);
        }
        return -1;
    }
#endif
};
}
} // namespace OHOS
#endif