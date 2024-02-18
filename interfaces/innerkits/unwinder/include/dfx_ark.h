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
#include <mutex>
#include <securec.h>
#include <string>

#include "dfx_frame.h"
#include "dfx_map.h"
#include "dfx_memory.h"

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
    int32_t line;
    int32_t column;
    uintptr_t codeBegin;
    uintptr_t codeSize;
    std::string ToString()
    {
        char buff[BUF_SIZE_MAX] = {0};
        (void)snprintf_s(buff, sizeof(buff), sizeof(buff), "%s:[begin:0x%" PRIx64 ", size:0x%" PRIx64 "]",
            functionName, codeBegin, codeSize);
        return std::string(buff);
    }
};
}
}

namespace OHOS {
namespace HiviewDFX {
using JsFrame = panda::ecmascript::JsFrame;
using JsFunction = panda::ecmascript::JsFunction;
using ReadMemFunc = panda::ecmascript::ReadMemFunc;

class DfxArk {
public:
    static int GetArkNativeFrameInfo(int pid, uintptr_t& pc, uintptr_t& fp, uintptr_t& sp,
                                     JsFrame* frames, size_t& size);
    static int StepArkManagedNativeFrame(int pid, uintptr_t& pc, uintptr_t& fp, uintptr_t& sp,
                                         char* buf, size_t bufSize);
    static int GetArkJsHeapCrashInfo(int pid, uintptr_t& x20, uintptr_t& fp, int outJsInfo, char* buf, size_t bufSize);

    static int StepArkFrame(void *obj, OHOS::HiviewDFX::ReadMemFunc readMemFn,
        uintptr_t *fp, uintptr_t *sp, uintptr_t *pc, bool *isJsFrame);

    static int ParseArkFrameInfo(uintptr_t byteCodePc, uintptr_t mapBase, uintptr_t loadOffset,
        uint8_t *data, uint64_t dataSize, JsFunction *jsFunction);
    static int TranslateArkFrameInfo(uint8_t *data, uint64_t dataSize, JsFunction *jsFunction);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
