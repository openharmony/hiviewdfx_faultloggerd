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

#ifndef DFX_ARK_H
#define DFX_ARK_H

#include <cstdint>
#include <securec.h>
#include <string>
#include <vector>

namespace panda {
namespace ecmascript {
constexpr uint16_t FUNCTIONNAME_MAX = 1024;
constexpr uint16_t URL_MAX = 1024;
constexpr uint16_t BUF_SIZE_MAX = FUNCTIONNAME_MAX + 32;

typedef bool (*ReadMemFunc)(void *, uintptr_t, uintptr_t *);

struct JsFrame {
    char functionName[FUNCTIONNAME_MAX];
    char url[URL_MAX];
    int32_t line;
    int32_t column;
};

struct JsFunction {
    char functionName[FUNCTIONNAME_MAX];
    char url[URL_MAX];
    int32_t line = 0;
    int32_t column = 0;
    uintptr_t codeBegin;
    uintptr_t codeSize;
    std::string ToString()
    {
        char buff[BUF_SIZE_MAX] = {0};
        if (snprintf_s(buff, sizeof(buff), sizeof(buff) - 1, "%s:[url:%s:%d:%d]",
            functionName, url, line, column) < 0) {
            return std::string("Unknown Function:[url:Unknown URL]");
        }
        return std::string(buff);
    }
};

struct ArkUnwindParam {
    void *ctx;
    ReadMemFunc readMem;
    uintptr_t *fp;
    uintptr_t *sp;
    uintptr_t *pc;
    uintptr_t *methodId;
    bool *isJsFrame;
    std::vector<uintptr_t>& jitCache;
    ArkUnwindParam(void *ctx, ReadMemFunc readMem, uintptr_t *fp, uintptr_t *sp, uintptr_t *pc,
        uintptr_t *methodId, bool *isJsFrame, std::vector<uintptr_t>& jitCache)
        : ctx(ctx), readMem(readMem), fp(fp), sp(sp), pc(pc),
          methodId(methodId), isJsFrame(isJsFrame), jitCache(jitCache) {}
};
}
}

namespace OHOS {
namespace HiviewDFX {
using JsFrame = panda::ecmascript::JsFrame;
using JsFunction = panda::ecmascript::JsFunction;
using ReadMemFunc = panda::ecmascript::ReadMemFunc;
using ArkUnwindParam = panda::ecmascript::ArkUnwindParam;

class DfxArk {
public:
    /**
     * @brief step ark frame
     *
     * @param obj memory pointer object
     * @param readMemFn read memory function
     * @param fp fp register
     * @param sp sp register
     * @param pc pc register
     * @param methodid methodid variable
     * @param isJsFrame isJsFrame variable
     * @return if succeed return 1, otherwise return -1
    */
    static int StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
        uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, uintptr_t* methodid, bool *isJsFrame);

    /**
     * @brief step ark frame with jit
     *
     * @param arkPrama ark param
     * @return if succeed return 1, otherwise return -1
    */
    static int StepArkFrameWithJit(OHOS::HiviewDFX::ArkUnwindParam* arkParam);

    /**
     * @brief jit code write file
     *
     * @param ctx memory pointer object
     * @param readMemFn read memory function
     * @param fd file descriptor
     * @param jitCodeArray jit code array
     * @param jitSize jit size
     * @return if succeed return 1, otherwise return -1
    */
    static int JitCodeWriteFile(void* ctx, OHOS::HiviewDFX::ReadMemFunc readMemFn, int fd,
        const uintptr_t* const jitCodeArray, const size_t jitSize);

    /**
     * @brief parse ark file info
     *
     * @param byteCodePc byteCode Pc
     * @param methodid method id
     * @param mapBase map base address
     * @param name map name
     * @param extractorPtr extractorPtr from ArkCreateJsSymbolExtractor
     * @param jsFunction jsFunction variable
     * @return if succeed return 1, otherwise return -1
    */
    static int ParseArkFileInfo(uintptr_t byteCodePc, uintptr_t methodid, uintptr_t mapBase, const char* name,
        uintptr_t extractorPtr, JsFunction *jsFunction);

    /**
     * @brief parse ark file info by local
     *
     * @param byteCodePc byteCode Pc
     * @param methodid method id
     * @param mapBase map base address
     * @param loadOffset map offset
     * @param jsFunction jsFunction variable
     * @return if succeed return 1, otherwise return -1
    */
    static int ParseArkFrameInfoLocal(uintptr_t byteCodePc, uintptr_t methodid,
        uintptr_t mapBase, uintptr_t loadOffset, JsFunction *jsFunction);

    /**
     * @brief Parse Ark Frame Info, for hiperf/hiProfile
     *
     * @param byteCodePc byteCode Pc
     * @param methodid method id
     * @param mapBase map base address
     * @param loadOffset map offset
     * @param data abc data
     * @param dataSize abc data size
     * @param extractorPtr extractorPtr from ArkCreateJsSymbolExtractor
     * @param jsFunction jsFunction variable
     * @return if succeed return 1, otherwise return -1
    */
    static int ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t loadOffset,
        uint8_t *data, uint64_t dataSize, uintptr_t extractorPtr, JsFunction *jsFunction);
    static int ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t methodid, uintptr_t mapBase, uintptr_t loadOffset,
        uint8_t *data, uint64_t dataSize, uintptr_t extractorPtr, JsFunction *jsFunction);

    /**
     * @brief create ark js symbol extracrot
     *
     * @param extractorPtr extractorPtr variable
     * @return if succeed return 1, otherwise return -1
    */
    static int ArkCreateJsSymbolExtractor(uintptr_t* extractorPtr);
    /**
     * @brief destory ark js symbol extracrot
     *
     * @param extractorPtr extractorPtr from ArkCreateJsSymbolExtractor
     * @return if succeed return 1, otherwise return -1
    */
    static int ArkDestoryJsSymbolExtractor(uintptr_t extractorPtr);

    /**
     * @brief create ark object by local
     *
     * @return if succeed return 1, otherwise return -1
    */
    static int ArkCreateLocal();
    /**
     * @brief destory ark object by local
     *
     * @return if succeed return 1, otherwise return -1
    */
    static int ArkDestroyLocal();
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
