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

#ifndef FP_BACKTRACE_H
#define FP_BACKTRACE_H

#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {
class FpBacktrace {
public:
    static FpBacktrace* CreateInstance();
    virtual ~FpBacktrace() = default;
    virtual uint32_t BacktraceFromFp(void* startFp, void** pcArray, uint32_t size) = 0;
    virtual DfxFrame* SymbolicAddress(void* pc) = 0;
};
}
}


#endif //FP_BACKTRACE_H
