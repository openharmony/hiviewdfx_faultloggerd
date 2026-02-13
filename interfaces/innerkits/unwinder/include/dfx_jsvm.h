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

#ifndef DFX_JSVM_H
#define DFX_JSVM_H

#include <cstdint>
#include <mutex>
#include <securec.h>
#include <string>
#include <vector>

namespace jsvm {
constexpr uint16_t FUNCTION_NAME_MAX = 1024;
constexpr uint16_t URL_MAX = 1024;
using ReadMemFunc = bool (*)(void *, uintptr_t, uintptr_t *);

struct JsvmFunction {
    char functionName[FUNCTION_NAME_MAX] = {0} ;
};

struct WebJsFunction {
    char functionName[FUNCTION_NAME_MAX] = {0};
    uint32_t offestInFunction = 0;
    char url[URL_MAX] = {0};
    int32_t line = 0;
    int32_t column = 0;
};

struct JsvmStepParam {
    uintptr_t *fp;
    uintptr_t *sp;
    uintptr_t *pc;
    bool *isJsFrame;

    JsvmStepParam(uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, bool *isJsFrame)
        : fp(fp), sp(sp), pc(pc), isJsFrame(isJsFrame) {}
};
}

namespace OHOS {
namespace HiviewDFX {
using JsvmFunction = jsvm::JsvmFunction;
using ReadMemFunc = jsvm::ReadMemFunc;
using JsvmStepParam = jsvm::JsvmStepParam;
using WebJsFunction = jsvm::WebJsFunction;

class DfxJsvm {
public:
    static DfxJsvm& Instance();
    /**
     * @brief step Jsvm frame
     *
     * @param obj memory pointer object
     * @param readMemFn read memory function
     * @param jsvmParam praram of step jsvm
     * @return if succeed return 1, otherwise return -1
    */
    int StepJsvmFrame(void *obj, ReadMemFunc readMemFn, JsvmStepParam* jsvmParam);
    /**
     * @brief Parse Jsvm Frame Info, for hiperf/hiProfile
     *
     * @param pc program counter
     * @param extractorPtr extractorPtr from JsvmCreateJsSymbolExtractor
     * @param JsvmFunction jsvmFunction variable
     * @return if succeed return 1, otherwise return -1
    */
    int ParseJsvmFrameInfo(uintptr_t pc, uintptr_t extractorPtr, JsvmFunction *jsvmFunction);

    /**
     * @brief create Jsvm js symbol extracrot
     *
     * @param extractorPtr extractorPtr variable
     * @param pid dump target process pid
     * @return if succeed return 1, otherwise return -1
    */
    int JsvmCreateJsSymbolExtractor(uintptr_t* extractorPtr, uint32_t pid);
    /**
     * @brief destroy Jsvm js symbol extracrot
     *
     * @param extractorPtr extractorPtr from JsvmCreateJsSymbolExtractor
     * @return if succeed return 1, otherwise return -1
    */
    int JsvmDestroyJsSymbolExtractor(uintptr_t extractorPtr);

        /**
     * @brief step arkweb js frame
     *
     * @param obj memory pointer object
     * @param readMemFn read memory function
     * @param jsvmParam praram of step jsvm
     * @return if succeed return 1, otherwise return -1
    */
    int StepArkwebJsFrame(void *obj, ReadMemFunc readMemFn, JsvmStepParam* jsvmParam);

    /**
     * @brief Parse arkweb js Frame Info
     *
     * @param pc program counter
     * @param extractorPtr extractorPtr from ArkwebCreateJsSymbolExtractor
     * @param webJsFunction webJsFunction variable
     * @return if succeed return 1, otherwise return -1
    */
    int ParseArkwebJsFrameInfo(uintptr_t pc, uintptr_t extractorPtr, WebJsFunction *webJsFunction);

    /**
     * @brief create arkweb js symbol extracrot
     *
     * @param extractorPtr extractorPtr variable
     * @param pid dump target process pid
     * @return if succeed return 1, otherwise return -1
    */
    int ArkwebCreateJsSymbolExtractor(uintptr_t* extractorPtr, uint32_t pid);
    /**
     * @brief destroy arkweb js symbol extracrot
     *
     * @param extractorPtr extractorPtr from ArkwebCreateJsSymbolExtractor
     * @return if succeed return 1, otherwise return -1
    */
    int ArkwebDestroyJsSymbolExtractor(uintptr_t extractorPtr);

private:
    DfxJsvm() = default;
    ~DfxJsvm() = default;
    DfxJsvm(const DfxJsvm&) = delete;
    DfxJsvm& operator=(const DfxJsvm&) = delete;
    std::string GetArkwebInstallLibPath();
    bool GetLibJsvmHandle(const char* const libName, void* &handle);
    bool DlsymJsvmFunc(const char* const libName, const char* const funcName, void* dlsymFuncPointer);
    bool InitJsvmFunction(const char* const functionName);
    using StepJsvmFn = int (*)(void*, ReadMemFunc, JsvmStepParam*);
    using ParseJsvmFrameInfoFn = int (*)(uintptr_t, uintptr_t, JsvmFunction*);
    using JsvmCreateJsSymbolExtractorFn = int (*)(uintptr_t*, uint32_t);
    using JsvmDestroyJsSymbolExtractorFn = int (*)(uintptr_t);
    std::mutex mutex_;

    StepJsvmFn stepJsvmFn_ = nullptr;
    ParseJsvmFrameInfoFn parseJsvmFrameInfoFn_ = nullptr;
    JsvmCreateJsSymbolExtractorFn jsvmCreateJsSymbolExtractorFn_ = nullptr;
    JsvmDestroyJsSymbolExtractorFn jsvmDestroyJsSymbolExtractorFn_ = nullptr;

    using StepArkwebJsFn = int (*)(void*, ReadMemFunc, JsvmStepParam*);
    using ParseArkwebJsFrameInfoFn = int (*)(uintptr_t, uintptr_t, WebJsFunction*);
    using ArkwebCreateJsSymbolExtractorFn = int (*)(uintptr_t*, uint32_t);
    using ArkwebDestroyJsSymbolExtractorFn = int (*)(uintptr_t);
    StepArkwebJsFn stepArkwebJsFn_ = nullptr;
    ParseArkwebJsFrameInfoFn parseArkwebJsFrameInfoFn_ = nullptr;
    ArkwebCreateJsSymbolExtractorFn arkwebCreateJsSymbolExtractorFn_ = nullptr;
    ArkwebDestroyJsSymbolExtractorFn arkwebDestroyJsSymbolExtractorFn_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
