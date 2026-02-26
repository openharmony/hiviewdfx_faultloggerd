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
const char* const ARK_WEB_JS_LIB_NAME = "libwebjs_dfx.so";
const char* const CREATE_JSVM_FUNC_NAME = "create_jsvm_extractor";
const char* const DESTROY_JSVM_FUNC_NAME = "destroy_jsvm_extractor";
const char* const PARSE_JSVM_FUNC_NAME = "jsvm_parse_js_frame_info";
const char* const STEP_JSVM_FUNC_NAME = "step_jsvm";

const char* const CREATE_ARKWEBJS_FUNC_NAME = "create_webjs_extractor";
const char* const DESTROY_ARKWEBJS_FUNC_NAME = "destroy_webjs_extractor";
const char* const PARSE_ARKWEBJS_FUNC_NAME = "web_parse_js_frame_info";
const char* const STEP_ARKWEBJS_FUNC_NAME = "step_webjs";
struct JsvmFunctionTable {
    const char* libPath;
    const char* const functionName;
    void* funcPointer;
};
}
std::string DfxJsvm::GetArkwebInstallLibPath()
{
    std::string bundleName = "com.huawei.hmos.arkwebcore";
#if defined(__aarch64__)
    const std::string arkwebEngineLibPath = "/data/app/el1/bundle/public/" + std::string(bundleName) + "/libs/arm64/";
#elif defined(__x86_64__)
    const std::string arkwebEngineLibPath = "";
#elif defined(__arm__)
    const std::string arkwebEngineLibPath = "/data/app/el1/bundle/public/" + std::string(bundleName) + "/libs/arm/";
#endif
    return arkwebEngineLibPath;
}

bool DfxJsvm::GetLibJsvmHandle(const char* const libName, void* &handle)
{
    handle = dlopen(libName, RTLD_LAZY);
    if (handle == nullptr) {
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

bool DfxJsvm::DlsymJsvmFunc(const char* const libName, const char* const funcName, void* dlsymFuncPointer)
{
    void* handle = nullptr;
    if (!GetLibJsvmHandle(libName, handle)) {
        return false;
    }
    *reinterpret_cast<void**>(dlsymFuncPointer) = dlsym(handle, funcName);
    if (dlsymFuncPointer == nullptr) {
        DFXLOGE("Failed to dlsym(%{public}s), error: %{public}s", funcName, dlerror());
        return false;
    }
    return true;
}

bool DfxJsvm::InitJsvmFunction(const char* const functionName)
{
    std::unique_lock<std::mutex> lock(mutex_);
    std::string arkWebLib = GetArkwebInstallLibPath() + std::string(ARK_WEB_JS_LIB_NAME);
    std::vector<JsvmFunctionTable> functionTable = {
        {JSVM_LIB_NAME, CREATE_JSVM_FUNC_NAME, reinterpret_cast<void*>(&jsvmCreateJsSymbolExtractorFn_)},
        {JSVM_LIB_NAME, DESTROY_JSVM_FUNC_NAME, reinterpret_cast<void*>(&jsvmDestroyJsSymbolExtractorFn_)},
        {JSVM_LIB_NAME, PARSE_JSVM_FUNC_NAME, reinterpret_cast<void*>(&parseJsvmFrameInfoFn_)},
        {JSVM_LIB_NAME, STEP_JSVM_FUNC_NAME, reinterpret_cast<void*>(&stepJsvmFn_)},
        {arkWebLib.c_str(), CREATE_ARKWEBJS_FUNC_NAME, reinterpret_cast<void*>(&arkwebCreateJsSymbolExtractorFn_)},
        {arkWebLib.c_str(), DESTROY_ARKWEBJS_FUNC_NAME, reinterpret_cast<void*>(&arkwebDestroyJsSymbolExtractorFn_)},
        {arkWebLib.c_str(), PARSE_ARKWEBJS_FUNC_NAME, reinterpret_cast<void*>(&parseArkwebJsFrameInfoFn_)},
        {arkWebLib.c_str(), STEP_ARKWEBJS_FUNC_NAME, reinterpret_cast<void*>(&stepArkwebJsFn_)},
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
    if (jsvmCreateJsSymbolExtractorFn_ != nullptr || InitJsvmFunction(CREATE_JSVM_FUNC_NAME)) {
        return jsvmCreateJsSymbolExtractorFn_(extractorPtr, pid);
    }
    return -1;
}

int DfxJsvm::JsvmDestroyJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (jsvmDestroyJsSymbolExtractorFn_ != nullptr || InitJsvmFunction(DESTROY_JSVM_FUNC_NAME)) {
        return jsvmDestroyJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxJsvm::ParseJsvmFrameInfo(uintptr_t pc, uintptr_t extractorPtr, JsvmFunction *jsvmFunction)
{
    if ((parseJsvmFrameInfoFn_ != nullptr || InitJsvmFunction(PARSE_JSVM_FUNC_NAME)) &&
        jsvmFunction != nullptr) {
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
    if (stepJsvmFn_ != nullptr || InitJsvmFunction(STEP_JSVM_FUNC_NAME)) {
        return stepJsvmFn_(obj, readMemFn, jsvmParam);
    }
    return -1;
}

int DfxJsvm::ArkwebCreateJsSymbolExtractor(uintptr_t* extractorPtr, uint32_t pid)
{
    if (arkwebCreateJsSymbolExtractorFn_ != nullptr ||
        InitJsvmFunction(CREATE_ARKWEBJS_FUNC_NAME)) {
        return arkwebCreateJsSymbolExtractorFn_(extractorPtr, pid);
    }
    return -1;
}

int DfxJsvm::ArkwebDestroyJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (arkwebDestroyJsSymbolExtractorFn_ != nullptr ||
        InitJsvmFunction(DESTROY_ARKWEBJS_FUNC_NAME)) {
        return arkwebDestroyJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxJsvm::ParseArkwebJsFrameInfo(uintptr_t pc, uintptr_t extractorPtr, WebJsFunction *webJsFunction)
{
    if ((parseArkwebJsFrameInfoFn_ != nullptr || InitJsvmFunction(PARSE_ARKWEBJS_FUNC_NAME)) &&
        webJsFunction != nullptr) {
        return parseArkwebJsFrameInfoFn_(pc, extractorPtr, webJsFunction);
    }
    return -1;
}

int DfxJsvm::StepArkwebJsFrame(void *obj, ReadMemFunc readMemFn, JsvmStepParam* jsvmParam)
{
    if (obj == nullptr || readMemFn == nullptr || jsvmParam == nullptr) {
        DFXLOGE("param is nullptr.");
        return -1;
    }
    if (stepArkwebJsFn_ != nullptr || InitJsvmFunction(STEP_ARKWEBJS_FUNC_NAME)) {
        return stepArkwebJsFn_(obj, readMemFn, jsvmParam);
    }
    return -1;
}
} // namespace HiviewDFX
} // namespace OHOS
