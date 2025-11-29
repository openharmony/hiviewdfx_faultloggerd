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

#include "dfx_frame.h"

#include <string>
#include "string_util.h"
#include "dfx_elf.h"
#include "dfx_map.h"

namespace OHOS {
namespace HiviewDFX {

#ifndef is_ohos_lite
std::string DfxFrame::ToSymbolString() const
{
    std::string output = StringPrintf("0x%016" PRIx64 " : ", pc);
    output.append(funcName);
    if (funcOffset != 0) {
        output += StringPrintf("[0x%016" PRIx64 ":0x%016" PRIx64 "][+0x%" PRIx64 "]",
            pc - mapOffset, funcOffset, mapOffset);
    }
    output.append("@");
    output.append(mapName);
    if (StartsWith(mapName, ADLT_PATH_NAME_START) && EndsWith(mapName, ".so")) {
        if (!originSoName.empty()) {
            output.append(":");
            output.append(originSoName);
        }
    }
    
    output.append(":");
    output.append(std::to_string(index));
    return output;
}
#endif

}  // namespace HiviewDFX
}  // namespace OHOS
