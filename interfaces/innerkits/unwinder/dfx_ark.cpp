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

const char ARK_LIB_NAME[] = "libark_jsruntime.so";
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

template <typename FuncName>
void DfxArk::DlsymArkFunc(const char* funcName, FuncName& dlsymFuncName)
{
    std::unique_lock<std::mutex> lock(arkMutex_);
    do {
        if (dlsymFuncName != nullptr) {
            break;
        }
        if (!GetLibArkHandle()) {
            break;
        }
        (dlsymFuncName) = reinterpret_cast<FuncName>(dlsym(handle_, (funcName)));
        if (dlsymFuncName == nullptr) {
            DFXLOGE("Failed to dlsym(%{public}s), error: %{public}s", funcName, dlerror());
            break;
        }
    } while (false);
}

int DfxArk::ArkCreateJsSymbolExtractor(uintptr_t* extractorPtr)
{
    if (arkCreateJsSymbolExtractorFn_ != nullptr) {
        return arkCreateJsSymbolExtractorFn_(extractorPtr);
    }

    const char* arkFuncName = "ark_create_js_symbol_extractor";
    DlsymArkFunc(arkFuncName, arkCreateJsSymbolExtractorFn_);

    if (arkCreateJsSymbolExtractorFn_ != nullptr) {
        return arkCreateJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkDestoryJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (arkDestoryJsSymbolExtractorFn_ != nullptr) {
        return arkDestoryJsSymbolExtractorFn_(extractorPtr);
    }

    const char* arkFuncName = "ark_destory_js_symbol_extractor";
    DlsymArkFunc(arkFuncName, arkDestoryJsSymbolExtractorFn_);

    if (arkDestoryJsSymbolExtractorFn_ != nullptr) {
        return arkDestoryJsSymbolExtractorFn_(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkCreateLocal()
{
    if (arkCreateLocalFn_ != nullptr) {
        return arkCreateLocalFn_();
    }

    const char* arkFuncName = "ark_create_local";
    DlsymArkFunc(arkFuncName, arkCreateLocalFn_);

    if (arkCreateLocalFn_ != nullptr) {
        return arkCreateLocalFn_();
    }
    return -1;
}

int DfxArk::ArkDestroyLocal()
{
    if (arkDestroyLocalFn_ != nullptr) {
        return arkDestroyLocalFn_();
    }

    const char* arkFuncName = "ark_destroy_local";
    DlsymArkFunc(arkFuncName, arkDestroyLocalFn_);

    if (arkDestroyLocalFn_ != nullptr) {
        return arkDestroyLocalFn_();
    }
    return -1;
}

int DfxArk::ParseArkFileInfo(uintptr_t byteCodePc, uintptr_t methodid, uintptr_t mapBase, const char* name,
    uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (parseArkFileInfoFn_ != nullptr) {
        return parseArkFileInfoFn_(byteCodePc, methodid, mapBase, name, extractorPtr, jsFunction);
    }

    const char* arkFuncName = "ark_parse_js_file_info";
    DlsymArkFunc(arkFuncName, parseArkFileInfoFn_);

    if (parseArkFileInfoFn_ != nullptr) {
        return parseArkFileInfoFn_(byteCodePc, methodid, mapBase, name, extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfoLocal(uintptr_t byteCodePc, uintptr_t methodid, uintptr_t mapBase,
    uintptr_t offset, JsFunction *jsFunction)
{
    if (parseArkFrameInfoLocalFn_ != nullptr) {
        return parseArkFrameInfoLocalFn_(byteCodePc, methodid, mapBase, offset, jsFunction);
    }

    const char* arkFuncName = "ark_parse_js_frame_info_local";
    DlsymArkFunc(arkFuncName, parseArkFrameInfoLocalFn_);

    if (parseArkFrameInfoLocalFn_ != nullptr) {
        return parseArkFrameInfoLocalFn_(byteCodePc, methodid, mapBase, offset, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t loadOffset,
    uint8_t *data, uint64_t dataSize, uintptr_t extractorPtr, JsFunction *jsFunction)
{
    return ParseArkFrameInfo(byteCodePc, 0, mapBase, loadOffset, data, dataSize, extractorPtr, jsFunction);
}

int DfxArk::ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t methodid, uintptr_t mapBase, uintptr_t loadOffset,
    uint8_t *data, uint64_t dataSize, uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (parseArkFrameInfoFn_ != nullptr) {
        return parseArkFrameInfoFn_(byteCodePc, methodid, mapBase, loadOffset, data, dataSize,
            extractorPtr, jsFunction);
    }

    const char* arkFuncName = "ark_parse_js_frame_info";
    DlsymArkFunc(arkFuncName, parseArkFrameInfoFn_);

    if (parseArkFrameInfoFn_ != nullptr) {
        return parseArkFrameInfoFn_(byteCodePc, methodid, mapBase, loadOffset, data, dataSize,
            extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
    uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, uintptr_t* methodid, bool *isJsFrame)
{
    if (stepArkFn_ != nullptr) {
        return stepArkFn_(obj, readMemFn, fp, sp, pc, methodid, isJsFrame);
    }

    const char* arkFuncName = "step_ark";
    DlsymArkFunc(arkFuncName, stepArkFn_);

    if (stepArkFn_ != nullptr) {
        return stepArkFn_(obj, readMemFn, fp, sp, pc, methodid, isJsFrame);
    }
    return -1;
}

int DfxArk::StepArkFrameWithJit(OHOS::HiviewDFX::ArkUnwindParam* arkParam)
{
    if (stepArkWithJitFn_ != nullptr) {
        return stepArkWithJitFn_(arkParam);
    }

    const char* const arkFuncName = "step_ark_with_record_jit";
    DlsymArkFunc(arkFuncName, stepArkWithJitFn_);

    if (stepArkWithJitFn_ != nullptr) {
        return stepArkWithJitFn_(arkParam);
    }
    return -1;
}

int DfxArk::JitCodeWriteFile(void* ctx, OHOS::HiviewDFX::ReadMemFunc readMemFn, int fd,
    const uintptr_t* const jitCodeArray, const size_t jitSize)
{
    if (jitCodeWriteFileFn_ != nullptr) {
        return jitCodeWriteFileFn_(ctx, readMemFn, fd, jitCodeArray, jitSize);
    }

    const char* const arkFuncName = "ark_write_jit_code";
    DlsymArkFunc(arkFuncName, jitCodeWriteFileFn_);

    if (jitCodeWriteFileFn_ != nullptr) {
        return jitCodeWriteFileFn_(ctx, readMemFn, fd, jitCodeArray, jitSize);
    }
    return -1;
}
} // namespace HiviewDFX
} // namespace OHOS
