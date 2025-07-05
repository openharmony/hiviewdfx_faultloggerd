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
}

bool DfxJsvm::GetLibJsvmHandle()
{
    if (handle_ != nullptr) {
        return true;
    }
    handle_ = dlopen(JSVM_LIB_NAME, RTLD_LAZY);
    if (handle_ == nullptr) {
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

template <typename FuncName>
void DfxJsvm::DlsymJsvmFunc(const char* funcName, FuncName& dlsymFuncName)
{
    std::unique_lock<std::mutex> lock(mutex_);
    do {
        if (dlsymFuncName != nullptr) {
            break;
        }
        if (!GetLibJsvmHandle()) {
            break;
        }
        (dlsymFuncName) = reinterpret_cast<FuncName>(dlsym(handle_, (funcName)));
        if (dlsymFuncName == nullptr) {
            DFXLOGE("Failed to dlsym(%{public}s), error: %{public}s", funcName, dlerror());
            break;
        }
    } while (false);
}

int DfxJsvm::JsvmCreateJsSymbolExtractor(uintptr_t* extractorPtr, uint32_t pid)
{
    if (jsvmCreateJsSymbolExtractorFn_ != nullptr) {
        return jsvmCreateJsSymbolExtractorFn_(extractorPtr, pid);
    }

    const char* jsvmFuncName = "create_jsvm_extractor";
    DlsymJsvmFunc(jsvmFuncName, jsvmCreateJsSymbolExtractorFn_);

    if (jsvmCreateJsSymbolExtractorFn_ != nullptr) {
        return jsvmCreateJsSymbolExtractorFn_(extractorPtr, pid);
    }
    return -1;
}

int DfxJsvm::JsvmDestroyJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (jsvmDestroyJsSymbolExtractorFn_ != nullptr) {
        return jsvmDestroyJsSymbolExtractorFn_(extractorPtr);
    }

    const char* jsvmFuncName = "destroy_jsvm_extractor";
    DlsymJsvmFunc(jsvmFuncName, jsvmDestroyJsSymbolExtractorFn_);

    if (jsvmDestroyJsSymbolExtractorFn_ != nullptr) {
        return jsvmDestroyJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxJsvm::ParseJsvmFrameInfo(uintptr_t pc, uintptr_t extractorPtr, JsvmFunction *jsvmFunction)
{
    if (parseJsvmFrameInfoFn_ != nullptr && jsvmFunction != nullptr) {
        return parseJsvmFrameInfoFn_(pc, extractorPtr, jsvmFunction);
    }

    const char* jsvmFuncName = "jsvm_parse_js_frame_info";
    DlsymJsvmFunc(jsvmFuncName, parseJsvmFrameInfoFn_);

    if (parseJsvmFrameInfoFn_ != nullptr && jsvmFunction != nullptr) {
        return parseJsvmFrameInfoFn_(pc, extractorPtr, jsvmFunction);
    }
    return -1;
}

int DfxJsvm::StepJsvmFrame(void *obj, ReadMemFunc readMemFn, JsvmStepParam* jsvmParam)
{
    if (obj == nullptr || readMemFn == nullptr || jsvmParam == nullptr) {
        DFXLOGE("param is nullptr.");
        return -1;
    }
    if (stepJsvmFn_ != nullptr) {
        return stepJsvmFn_(obj, readMemFn, jsvmParam);
    }

    const char* jsvmFuncName = "step_jsvm";
    DlsymJsvmFunc(jsvmFuncName, stepJsvmFn_);

    if (stepJsvmFn_ != nullptr) {
        return stepJsvmFn_(obj, readMemFn, jsvmParam);
    }
    return -1;
}
} // namespace HiviewDFX
} // namespace OHOS
