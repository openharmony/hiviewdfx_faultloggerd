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
}

bool DfxArk::GetLibArkHandle()
{
    if (handle_ != nullptr) {
        return true;
    }
    handle_ = dlopen(ARK_LIB_NAME, RTLD_LAZY);
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

bool DfxArk::DlsymArkFunc(const char* funcName, void** dlsymFuncName)
{
    if (dlsymFuncName == nullptr) {
        return false;
    }
    if (*dlsymFuncName != nullptr) {
        return true;
    }
    if (!GetLibArkHandle()) {
        return false;
    }
    *dlsymFuncName = dlsym(handle_, (funcName));
    if (*dlsymFuncName == nullptr) {
        DFXLOGE("Failed to dlsym(%{public}s), error: %{public}s", funcName, dlerror());
        return false;
    }
    return true;
}

bool DfxArk::InitArkFunction(ArkFunction arkFunction)
{
    std::unique_lock<std::mutex> lock(arkMutex_);
    switch (arkFunction) {
        case ArkFunction::ARK_CREATE_JS_SYMBOL_EXTRACTOR:
            return DlsymArkFunc("ark_create_js_symbol_extractor",
                reinterpret_cast<void**>(&arkCreateJsSymbolExtractorFn_));
        case ArkFunction::ARK_DESTORY_JS_SYMBOL_EXTRACTOR:
            return DlsymArkFunc("ark_destory_js_symbol_extractor",
                reinterpret_cast<void**>(&arkDestoryJsSymbolExtractorFn_));
        case ArkFunction::ARK_CREATE_LOCAL:
            return DlsymArkFunc("ark_create_local", reinterpret_cast<void**>(&arkCreateLocalFn_));
        case ArkFunction::ARK_DESTROY_LOCAL:
            return DlsymArkFunc("ark_destroy_local", reinterpret_cast<void**>(&arkDestroyLocalFn_));
        case ArkFunction::ARK_PARSE_JS_FILE_INFO:
            return DlsymArkFunc("ark_parse_js_file_info", reinterpret_cast<void**>(&parseArkFileInfoFn_));
        case ArkFunction::ARK_PARSE_JS_FRAME_INFO_LOCAL:
            return DlsymArkFunc("ark_parse_js_frame_info_local", reinterpret_cast<void**>(&parseArkFrameInfoLocalFn_));
        case ArkFunction::ARK_PARSE_JS_FRAME_INFO:
            return DlsymArkFunc("ark_parse_js_frame_info", reinterpret_cast<void**>(&parseArkFrameInfoFn_));
        case ArkFunction::STEP_ARK:
            return DlsymArkFunc("step_ark", reinterpret_cast<void**>(&stepArkFn_));
        case ArkFunction::STEP_ARK_WITH_RECORD_JIT:
            return DlsymArkFunc("step_ark_with_record_jit", reinterpret_cast<void**>(&stepArkWithJitFn_));
        case ArkFunction::ARK_WRITE_JIT_CODE:
            return DlsymArkFunc("ark_write_jit_code", reinterpret_cast<void**>(&jitCodeWriteFileFn_));
        default:
            return false;
    }
}

int DfxArk::ArkCreateJsSymbolExtractor(uintptr_t* extractorPtr)
{
    if (arkCreateJsSymbolExtractorFn_ != nullptr || InitArkFunction(ArkFunction::ARK_CREATE_JS_SYMBOL_EXTRACTOR)) {
        return arkCreateJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkDestoryJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (arkDestoryJsSymbolExtractorFn_ != nullptr || InitArkFunction(ArkFunction::ARK_DESTORY_JS_SYMBOL_EXTRACTOR)) {
        return arkDestoryJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkCreateLocal()
{
    if (arkCreateLocalFn_ != nullptr || InitArkFunction(ArkFunction::ARK_CREATE_LOCAL)) {
        return arkCreateLocalFn_();
    }
    return -1;
}

int DfxArk::ArkDestroyLocal()
{
    if (arkDestroyLocalFn_ != nullptr || InitArkFunction(ArkFunction::ARK_DESTROY_LOCAL)) {
        return arkDestroyLocalFn_();
    }
    return -1;
}

int DfxArk::ParseArkFileInfo(uintptr_t byteCodePc, uintptr_t mapBase, const char* name,
    uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (parseArkFileInfoFn_ != nullptr || InitArkFunction(ArkFunction::ARK_PARSE_JS_FILE_INFO)) {
        return parseArkFileInfoFn_(byteCodePc, mapBase, name, extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfoLocal(uintptr_t byteCodePc, uintptr_t mapBase,
    uintptr_t offset, JsFunction *jsFunction)
{
    if (parseArkFrameInfoLocalFn_ != nullptr || InitArkFunction(ArkFunction::ARK_PARSE_JS_FRAME_INFO_LOCAL)) {
        return parseArkFrameInfoLocalFn_(byteCodePc, mapBase, offset, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t loadOffset,
    uint8_t *data, uint64_t dataSize, uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (parseArkFrameInfoFn_ != nullptr || InitArkFunction(ArkFunction::ARK_PARSE_JS_FRAME_INFO)) {
        return parseArkFrameInfoFn_(byteCodePc, mapBase, loadOffset, data, dataSize,
            extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
    OHOS::HiviewDFX::ArkStepParam* arkParam)
{
    if (stepArkFn_ != nullptr || InitArkFunction(ArkFunction::STEP_ARK)) {
        return stepArkFn_(obj, readMemFn, arkParam);
    }
    return -1;
}

int DfxArk::StepArkFrameWithJit(OHOS::HiviewDFX::ArkUnwindParam* arkParam)
{
    if (stepArkWithJitFn_ != nullptr || InitArkFunction(ArkFunction::STEP_ARK_WITH_RECORD_JIT)) {
        return stepArkWithJitFn_(arkParam);
    }
    return -1;
}

int DfxArk::JitCodeWriteFile(void* ctx, OHOS::HiviewDFX::ReadMemFunc readMemFn, int fd,
    const uintptr_t* const jitCodeArray, const size_t jitSize)
{
    if (jitCodeWriteFileFn_ != nullptr || InitArkFunction(ArkFunction::ARK_WRITE_JIT_CODE)) {
        return jitCodeWriteFileFn_(ctx, readMemFn, fd, jitCodeArray, jitSize);
    }
    return -1;
}
} // namespace HiviewDFX
} // namespace OHOS
