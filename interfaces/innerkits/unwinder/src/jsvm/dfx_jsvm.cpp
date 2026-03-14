/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "dfx_jsvm.h"

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
#define LOG_TAG "DfxJsvm"

const char* const JSVM_LIB_NAME = "libjsvm.so";
const char* const CREATE_JSVM_FUNC_NAME = "create_jsvm_extractor";
const char* const DESTROY_JSVM_FUNC_NAME = "destroy_jsvm_extractor";
const char* const PARSE_JSVM_FUNC_NAME = "jsvm_parse_js_frame_info";
const char* const STEP_JSVM_FUNC_NAME = "step_jsvm";
struct JsvmFunctionTable {
    const char* libPath;
    const char* const functionName;
    void* funcPointer;
};
}
bool DfxJsvm::GetLibJsvmHandle(const char* const libName)
{
    if (jsvmHandle_ != nullptr) {
        return true;
    }
    jsvmHandle_ = dlopen(libName, RTLD_LAZY);
    if (jsvmHandle_ == nullptr) {
        DFXLOGU("Failed to load library(%{public}s).", dlerror());
        return false;
    }
    return true;
}

DfxJsvm& DfxJsvm::Instance()
{
    static DfxJsvm instance;
    return instance;
}

void DfxJsvm::Clear()
{
    pthread_rwlock_wrlock(&rwLock_);
    if (jsvmHandle_ != nullptr) {
        dlclose(jsvmHandle_);
        jsvmHandle_ = nullptr;
    }
    jsvmCreateJsSymbolExtractorFn_ = nullptr;
    jsvmDestroyJsSymbolExtractorFn_ = nullptr;
    parseJsvmFrameInfoFn_ = nullptr;
    stepJsvmFn_ = nullptr;
    pthread_rwlock_unlock(&rwLock_);
}

bool DfxJsvm::DlsymJsvmFunc(const char* const libName, const char* const funcName, void* dlsymFuncPointer)
{
    if (!GetLibJsvmHandle(libName)) {
        return false;
    }
    auto func = dlsym(jsvmHandle_, funcName);
    if (func == nullptr) {
        DFXLOGE("Failed to dlsym(%{public}s), error: %{public}s", funcName, dlerror());
        return false;
    }
    *reinterpret_cast<void**>(dlsymFuncPointer) = func;
    return true;
}

bool DfxJsvm::InitJsvmFunction(const char* const functionName)
{
    std::unique_lock<std::mutex> lock(mutex_);
    std::vector<JsvmFunctionTable> functionTable = {
        {JSVM_LIB_NAME, CREATE_JSVM_FUNC_NAME, reinterpret_cast<void*>(&jsvmCreateJsSymbolExtractorFn_)},
        {JSVM_LIB_NAME, DESTROY_JSVM_FUNC_NAME, reinterpret_cast<void*>(&jsvmDestroyJsSymbolExtractorFn_)},
        {JSVM_LIB_NAME, PARSE_JSVM_FUNC_NAME, reinterpret_cast<void*>(&parseJsvmFrameInfoFn_)},
        {JSVM_LIB_NAME, STEP_JSVM_FUNC_NAME, reinterpret_cast<void*>(&stepJsvmFn_)},
    };
    for (auto& function : functionTable) {
        if (strcmp(function.functionName, functionName) == 0) {
            return DlsymJsvmFunc(function.libPath, functionName, function.funcPointer);
        }
    }
    return false;
}

int DfxJsvm::JsvmCreateJsSymbolExtractor(uintptr_t* extractorPtr, uint32_t pid)
{
    int ret = -1;
    pthread_rwlock_rdlock(&rwLock_);
    if (jsvmCreateJsSymbolExtractorFn_ != nullptr || InitJsvmFunction(CREATE_JSVM_FUNC_NAME)) {
        ret = jsvmCreateJsSymbolExtractorFn_(extractorPtr, pid);
    }
    pthread_rwlock_unlock(&rwLock_);
    return ret;
}

int DfxJsvm::JsvmDestroyJsSymbolExtractor(uintptr_t extractorPtr)
{
    int ret = -1;
    pthread_rwlock_rdlock(&rwLock_);
    if (jsvmDestroyJsSymbolExtractorFn_ != nullptr || InitJsvmFunction(DESTROY_JSVM_FUNC_NAME)) {
        ret = jsvmDestroyJsSymbolExtractorFn_(extractorPtr);
    }
    pthread_rwlock_unlock(&rwLock_);
    return ret;
}

int DfxJsvm::ParseJsvmFrameInfo(uintptr_t pc, uintptr_t extractorPtr, JsvmFunction *jsvmFunction)
{
    int ret = -1;
    pthread_rwlock_rdlock(&rwLock_);
    if ((parseJsvmFrameInfoFn_ != nullptr || InitJsvmFunction(PARSE_JSVM_FUNC_NAME)) &&
        jsvmFunction != nullptr) {
        ret = parseJsvmFrameInfoFn_(pc, extractorPtr, jsvmFunction);
    }
    pthread_rwlock_unlock(&rwLock_);
    return ret;
}

int DfxJsvm::StepJsvmFrame(void *obj, ReadMemFunc readMemFn, JsvmStepParam* jsvmParam)
{
    if (obj == nullptr || readMemFn == nullptr || jsvmParam == nullptr) {
        DFXLOGE("param is nullptr.");
        return -1;
    }
    pthread_rwlock_rdlock(&rwLock_);
    int ret = -1;
    if (stepJsvmFn_ != nullptr || InitJsvmFunction(STEP_JSVM_FUNC_NAME)) {
        ret = stepJsvmFn_(obj, readMemFn, jsvmParam);
    }
    pthread_rwlock_unlock(&rwLock_);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
