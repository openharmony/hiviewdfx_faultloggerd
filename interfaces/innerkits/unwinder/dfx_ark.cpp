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

const char ARK_LIB_NAME[] = "libark_jsruntime.so";

void* g_handle = nullptr;
pthread_mutex_t g_mutex;
int (*g_getArkNativeFrameInfoFn)(int, uintptr_t*, uintptr_t*, uintptr_t*, JsFrame*, size_t&);
int (*g_stepArkManagedNativeFrameFn)(int, uintptr_t*, uintptr_t*, uintptr_t*, char*, size_t);
int (*g_getArkJsHeapCrashInfoFn)(int, uintptr_t*, uintptr_t*, int, char*, size_t);
int (*g_stepArkFn)(void*, OHOS::HiviewDFX::ReadMemFunc, uintptr_t*, uintptr_t*, uintptr_t*, uintptr_t*, bool*);
int (*g_parseArkFileInfoFn)(uintptr_t, uintptr_t, uintptr_t, const char*, uintptr_t, JsFunction*);
int (*g_parseArkFrameInfoLocalFn)(uintptr_t, uintptr_t, uintptr_t, JsFunction*);
int (*g_parseArkFrameInfoFn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint8_t*, uint64_t, uintptr_t, JsFunction*);
int (*g_translateArkFrameInfoFn)(uint8_t*, uint64_t, JsFunction*);
int (*g_arkCreateJsSymbolExtractorFn)(uintptr_t*);
int (*g_arkDestoryJsSymbolExtractorFn)(uintptr_t);
int (*g_arkDestoryLocalFn)();

bool GetLibArkHandle()
{
    if (g_handle != nullptr) {
        return true;
    }
    g_handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
    if (g_handle == nullptr) {
        LOGU("Failed to load library(%s).", dlerror());
        return false;
    }
    return true;
}
}

#define DLSYM_ARK_FUNC(FuncName, DlsymFuncName) { \
    pthread_mutex_lock(&g_mutex); \
    do { \
        if ((DlsymFuncName) != nullptr) { \
            break; \
        } \
        if (!GetLibArkHandle()) { \
            break; \
        } \
        *(void**)(&(DlsymFuncName)) = dlsym(g_handle, (FuncName)); \
        if ((DlsymFuncName) == NULL) { \
            LOGE("Failed to dlsym(%s), error: %s", (FuncName), dlerror()); \
            break; \
        } \
    } while (false); \
    pthread_mutex_unlock(&g_mutex); \
}

int DfxArk::ArkCreateJsSymbolExtractor(uintptr_t* extractorPtr)
{
    if (g_arkCreateJsSymbolExtractorFn != nullptr) {
        return g_arkCreateJsSymbolExtractorFn(extractorPtr);
    }

    const char* arkFuncName = "ark_create_js_symbol_extractor";
    DLSYM_ARK_FUNC(arkFuncName, g_arkCreateJsSymbolExtractorFn)

    if (g_arkCreateJsSymbolExtractorFn != nullptr) {
        return g_arkCreateJsSymbolExtractorFn(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkDestoryJsSymbolExtractor(uintptr_t extractorPtr)
{
    if (g_arkDestoryJsSymbolExtractorFn != nullptr) {
        return g_arkDestoryJsSymbolExtractorFn(extractorPtr);
    }

    const char* arkFuncName = "ark_destory_js_symbol_extractor";
    DLSYM_ARK_FUNC(arkFuncName, g_arkDestoryJsSymbolExtractorFn)

    if (g_arkDestoryJsSymbolExtractorFn != nullptr) {
        return g_arkDestoryJsSymbolExtractorFn(extractorPtr);
    }
    return -1;
}

int DfxArk::ArkDestoryLocal()
{
    if (g_arkDestoryLocalFn != nullptr) {
        return g_arkDestoryLocalFn();
    }

    const char* arkFuncName = "ark_destory_local";
    DLSYM_ARK_FUNC(arkFuncName, g_arkDestoryLocalFn)

    if (g_arkDestoryLocalFn != nullptr) {
        return g_arkDestoryLocalFn();
    }
    return -1;
}

int DfxArk::ParseArkFileInfo(uintptr_t byteCodePc, uintptr_t methodid, uintptr_t mapBase, const char* name,
    uintptr_t extractorPtr, JsFunction *jsFunction)
{
    if (g_parseArkFileInfoFn != nullptr) {
        return g_parseArkFileInfoFn(byteCodePc, methodid, mapBase, name, extractorPtr, jsFunction);
    }

    const char* arkFuncName = "ark_parse_js_file_info";
    DLSYM_ARK_FUNC(arkFuncName, g_parseArkFileInfoFn)

    if (g_parseArkFileInfoFn != nullptr) {
        return g_parseArkFileInfoFn(byteCodePc, methodid, mapBase, name, extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::ParseArkFrameInfoLocal(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t offset, JsFunction *jsFunction)
{
    if (g_parseArkFrameInfoLocalFn != nullptr) {
        return g_parseArkFrameInfoLocalFn(byteCodePc, mapBase, offset, jsFunction);
    }

    const char* arkFuncName = "ark_parse_js_frame_info_local";
    DLSYM_ARK_FUNC(arkFuncName, g_parseArkFrameInfoLocalFn)

    if (g_parseArkFrameInfoLocalFn != nullptr) {
        return g_parseArkFrameInfoLocalFn(byteCodePc, mapBase, offset, jsFunction);
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
    if (g_parseArkFrameInfoFn != nullptr) {
        return g_parseArkFrameInfoFn(byteCodePc, methodid, mapBase, loadOffset, data, dataSize, extractorPtr, jsFunction);
    }

    const char* arkFuncName = "ark_parse_js_frame_info";
    DLSYM_ARK_FUNC(arkFuncName, g_parseArkFrameInfoFn)

    if (g_parseArkFrameInfoFn != nullptr) {
        return g_parseArkFrameInfoFn(byteCodePc, methodid, mapBase, loadOffset, data, dataSize, extractorPtr, jsFunction);
    }
    return -1;
}

int DfxArk::TranslateArkFrameInfo(uint8_t *data, uint64_t dataSize, JsFunction *jsFunction)
{
    if (g_translateArkFrameInfoFn != nullptr) {
        return g_translateArkFrameInfoFn(data, dataSize, jsFunction);
    }

    const char* arkFuncName = "ark_translate_js_frame_info";
    DLSYM_ARK_FUNC(arkFuncName, g_translateArkFrameInfoFn)

    if (g_translateArkFrameInfoFn != nullptr) {
        return g_translateArkFrameInfoFn(data, dataSize, jsFunction);
    }
    return -1;
}

int DfxArk::StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
    uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, uintptr_t* methodid, bool *isJsFrame)
{
    if (g_stepArkFn != nullptr) {
        return g_stepArkFn(obj, readMemFn, fp, sp, pc, methodid, isJsFrame);
    }

    const char* arkFuncName = "step_ark";
    DLSYM_ARK_FUNC(arkFuncName, g_stepArkFn)

    if (g_stepArkFn != nullptr) {
        return g_stepArkFn(obj, readMemFn, fp, sp, pc, methodid, isJsFrame);
    }
    return -1;
}

int DfxArk::GetArkNativeFrameInfo(int pid, uintptr_t& pc, uintptr_t& fp, uintptr_t& sp, JsFrame* frames, size_t& size)
{
    if (g_getArkNativeFrameInfoFn != nullptr) {
        return g_getArkNativeFrameInfoFn(pid, &pc, &fp, &sp, frames, size);
    }

    const char* arkFuncName = "get_ark_native_frame_info";
    DLSYM_ARK_FUNC(arkFuncName, g_getArkNativeFrameInfoFn)

    if (g_getArkNativeFrameInfoFn != nullptr) {
        return g_getArkNativeFrameInfoFn(pid, &pc, &fp, &sp, frames, size);
    }
    return -1;
}

int DfxArk::StepArkManagedNativeFrame(int pid, uintptr_t& pc, uintptr_t& fp, uintptr_t& sp, char* buf, size_t bufSize)
{
    if (g_stepArkManagedNativeFrameFn != nullptr) {
        return g_stepArkManagedNativeFrameFn(pid, &pc, &fp, &sp, buf, bufSize);
    }

    const char* arkFuncName = "step_ark_managed_native_frame";
    DLSYM_ARK_FUNC(arkFuncName, g_stepArkManagedNativeFrameFn)

    if (g_stepArkManagedNativeFrameFn != nullptr) {
        return g_stepArkManagedNativeFrameFn(pid, &pc, &fp, &sp, buf, bufSize);
    }
    return -1;
}

int DfxArk::GetArkJsHeapCrashInfo(int pid, uintptr_t& x20, uintptr_t& fp, int outJsInfo, char* buf, size_t bufSize)
{
    if (g_getArkJsHeapCrashInfoFn != nullptr) {
        return g_getArkJsHeapCrashInfoFn(pid, &x20, &fp, outJsInfo, buf, bufSize);
    }

    const char* arkFuncName = "get_ark_js_heap_crash_info";
    DLSYM_ARK_FUNC(arkFuncName, g_getArkJsHeapCrashInfoFn)

    if (g_getArkJsHeapCrashInfoFn != nullptr) {
        return g_getArkJsHeapCrashInfoFn(pid, &x20, &fp, outJsInfo, buf, bufSize);
    }
    return -1;
}
} // namespace HiviewDFX
} // namespace OHOS
