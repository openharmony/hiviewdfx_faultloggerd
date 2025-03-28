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

#ifndef STRING_PRINTF_H
#define STRING_PRINTF_H

#include <cstdio>
#include <string>
#include <securec.h>

namespace OHOS {
namespace HiviewDFX {

inline int BufferAppendV(char *buf, int size, const char *fmt, va_list ap)
{
    if (buf == nullptr || size <= 0) {
        return -1;
    }
    int ret = vsnprintf_s(buf, size, size - 1, fmt, ap);
    return ret;
}

inline bool StringAppendV(std::string& dst, const char* fmt, va_list ap)
{
    constexpr int stringBufLen = 4096;
    char buffer[stringBufLen] = {0};
    va_list bakAp;
    va_copy(bakAp, ap);
    int ret = BufferAppendV(buffer, sizeof(buffer), fmt, bakAp);
    va_end(bakAp);
    if (ret > 0) {
        dst.append(buffer, ret);
    }
    return ret != -1;
}

inline int BufferPrintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = BufferAppendV(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

inline std::string StringPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

inline std::string StringPrintf(const char *fmt, ...)
{
    if (fmt == nullptr) {
        return "";
    }
    std::string dst;
    va_list ap;
    va_start(ap, fmt);
    StringAppendV(dst, fmt, ap);
    va_end(ap);
    return dst;
}

inline void StringAppendF(std::string& dst, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    StringAppendV(dst, fmt, ap);
    va_end(ap);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
