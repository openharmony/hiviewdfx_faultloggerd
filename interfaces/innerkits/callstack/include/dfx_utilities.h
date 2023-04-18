/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef OHOS_HIVIEWDFX_UTILITIES_H
#define OHOS_HIVIEWDFX_UTILITIES_H

#include <linux/types.h>
#include <securec.h>
#include <string>

using s8 = __s8;
using u8 = __u8;
using s16 = __s16;
using u16 = __u16;
using s32 = __s32;
using u32 = __u32;
using s64 = __s64;
using u64 = __u64;

constexpr const int DFX_DEFAULT_STRING_BUF_SIZE = 4096;

namespace OHOS {
namespace HiviewDFX {
const std::string EMPTY_STRING = "";

bool StringStartsWith(const std::string &string, const std::string &with);

template<typename... VA>
std::string StringPrintf(const char *stringFormat, VA... args)
{
    // check howmany bytes we need
    char bytes[DFX_DEFAULT_STRING_BUF_SIZE];
    bytes[DFX_DEFAULT_STRING_BUF_SIZE - 1] = '\0';

    if (stringFormat == nullptr) {
        return EMPTY_STRING;
    }

    // print it to bytes
    if (snprintf_s(bytes, sizeof(bytes), sizeof(bytes) - 1, stringFormat,
                   args...) < 0) {
        return EMPTY_STRING;
    }

    // make a string return
    return std::string(bytes);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif // OHOS_HIVIEWDFX_UTILITIES_H
