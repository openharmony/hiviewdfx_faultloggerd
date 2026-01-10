/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

constexpr auto ARK_LIB_NAME = "libark_jsruntime.so";
const char* const ARK_CREATE_JS_SYMBOL_EXTRACTOR = "ark_create_js_symbol_extractor";
const char* const ARK_DESTORY_JS_SYMBOL_EXTRACTOR = "ark_destory_js_symbol_extractor";
const char* const ARK_CREATE_LOCAL = "ark_create_local";
const char* const ARK_DESTROY_LOCAL = "ark_destroy_local";
const char* const ARK_PARSE_JS_FILE_INFO = "ark_parse_js_file_info";
const char* const ARK_PARSE_JS_FRAME_INFO_LOCAL = "ark_parse_js_frame_info_local";
const char* const ARK_PARSE_JS_FRAME_INFO = "ark_parse_js_frame_info";
const char* const STEP_ARK = "step_ark";
const char* const STEP_ARK_WITH_RECORD_JIT = "step_ark_with_record_jit";
const char* const ARK_WRITE_JIT_CODE = "ark_write_jit_code";

struct ArkFunctionTable {
    const char* const libName;
    const char* const functionName;
    void** funcPointer;
};
}

bool DfxArk::GetLibArkHandle(const char* const libName)
{
    if (handle_ != nullptr) {
        return true;
    }
    handle_ = dlopen(libName, RTLD_LAZY);
    if (handle_ == nullptr) {
        DFXLOGU("Failed to load library(%{public}s).", dlerror());
        return false;
    }
    return true;
}

DfxArk& DfxArk::Instance()
{
    static DfxArk instance;
    return instance;
}

bool DfxArk::DlsymArkFunc(const char* const libName, const char* const funcName, void** dlsymFuncName)
{
    if (dlsymFuncName == nullptr) {
        return false;
    }
    if (*dlsymFuncName != nullptr) {
        return true;
    }

    if (!GetLibArkHandle(libName)) {
        return false;
    }
    *dlsymFuncName = dlsym(handle_, (funcName));
    if (*dlsymFuncName == nullptr) {
        DFXLOGE("Failed to dlsym(%{public}s), error: %{public}s", funcName, dlerror());
        return false;
    }
    return true;
}

bool DfxArk::InitArkFunction(const char* const functionName)
{
    std::unique_lock<std::mutex> lock(arkMutex_);
    std::vector<ArkFunctionTable> functionTable = {
        {ARK_LIB_NAME, ARK_CREATE_JS_SYMBOL_EXTRACTOR, reinterpret_cast<void**>(&arkCreateJsSymbolExtractorFn_)},
        {ARK_LIB_NAME, ARK_DESTORY_JS_SYMBOL_EXTRACTOR, reinterpret_cast<void**>(&arkDestoryJsSymbolExtractorFn_)},
        {ARK_LIB_NAME, ARK_CREATE_LOCAL, reinterpret_cast<void**>(&arkCreateLocalFn_)},
        {ARK_LIB_NAME, ARK_DESTROY_LOCAL, reinterpret_cast<void**>(&arkDestroyLocalFn_)},
        {ARK_LIB_NAME, ARK_PARSE_JS_FILE_INFO, reinterpret_cast<void**>(&parseArkFileInfoFn_)},
        {ARK_LIB_NAME, ARK_PARSE_JS_FRAME_INFO_LOCAL, reinterpret_cast<void**>(&parseArkFrameInfoLocalFn_)},
        {ARK_LIB_NAME, ARK_PARSE_JS_FRAME_INFO, reinterpret_cast<void**>(&parseArkFrameInfoFn_)},
        {ARK_LIB_NAME, STEP_ARK, reinterpret_cast<void**>(&stepArkFn_)},
        {ARK_LIB_NAME, STEP_ARK_WITH_RECORD_JIT, reinterpret_cast<void**>(&stepArkWithJitFn_)},
        {ARK_LIB_NAME, ARK_WRITE_JIT_CODE, reinterpret_cast<void**>(&jitCodeWriteFileFn_)},
    };
    for (const auto& function : functionTable) {
        if (strcmp(function.functionName, functionName) == 0) {
            return DlsymArkFunc(function.libName, functionName, function.funcPointer);
        }
    }
    return false;
}

int DfxArk::ArkCreateJsSymbolExtractor(uintptr_t* extractorPtr)
{
    if (arkCreateJsSymbolExtractorFn_ != nullptr || InitArkFunction(ARK_CREATE_JS_SYMBOL_EXTRACTOR)) {
        return arkCreateJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkDestoryJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (arkDestoryJsSymbolExtractorFn_ != nullptr || InitArkFunction(ARK_DESTORY_JS_SYMBOL_EXTRACTOR)) {
        return arkDestoryJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkCreateLocal()
{
    if (arkCreateLocalFn_ != nullptr || InitArkFunction(ARK_CREATE_LOCAL)) {
        return arkCreateLocalFn_();
    }
    return -1;
}

int DfxArk::ArkDestroyLocal()
{
    if (arkDestroyLocalFn_ != nullptr || InitArkFunction(ARK_DESTROY_LOCAL)) {
        return arkDestroyLocalFn_();
    }
    return -1;
}

int DfxArk::ParseArkFileInfo(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t offset, const char* name,
    uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (parseArkFileInfoFn_ != nullptr || InitArkFunction(ARK_PARSE_JS_FILE_INFO)) {
        return parseArkFileInfoFn_(byteCodePc, mapBase, offset, name, extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfoLocal(uintptr_t byteCodePc, uintptr_t mapBase,
    uintptr_t offset, JsFunction *jsFunction)
{
    if (parseArkFrameInfoLocalFn_ != nullptr || InitArkFunction(ARK_PARSE_JS_FRAME_INFO_LOCAL)) {
        return parseArkFrameInfoLocalFn_(byteCodePc, mapBase, offset, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t loadOffset,
    uint8_t *data, uint64_t dataSize, uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (parseArkFrameInfoFn_ != nullptr || InitArkFunction(ARK_PARSE_JS_FRAME_INFO)) {
        return parseArkFrameInfoFn_(byteCodePc, mapBase, loadOffset, data, dataSize,
            extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
    OHOS::HiviewDFX::ArkStepParam* arkParam)
{
    if (stepArkFn_ != nullptr || InitArkFunction(STEP_ARK)) {
        return stepArkFn_(obj, readMemFn, arkParam);
    }
    return -1;
}

int DfxArk::StepArkFrameWithJit(OHOS::HiviewDFX::ArkUnwindParam* arkParam)
{
    if (stepArkWithJitFn_ != nullptr || InitArkFunction(STEP_ARK_WITH_RECORD_JIT)) {
        return stepArkWithJitFn_(arkParam);
    }
    return -1;
}

int DfxArk::JitCodeWriteFile(void* ctx, OHOS::HiviewDFX::ReadMemFunc readMemFn, int fd,
    const uintptr_t* const jitCodeArray, const size_t jitSize)
{
    if (jitCodeWriteFileFn_ != nullptr || InitArkFunction(ARK_WRITE_JIT_CODE)) {
        return jitCodeWriteFileFn_(ctx, readMemFn, fd, jitCodeArray, jitSize);
    }
    return -1;
}
} // namespace HiviewDFX
} // namespace OHOS
