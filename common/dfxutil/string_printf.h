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

#include <cstdint>
#include <cstdio>
#include <string>
#include <securec.h>

namespace OHOS {
namespace HiviewDFX {
namespace {
const int STRING_BUF_LEN = 1024;
}

static int BufferAppendV(char *buf, int size, const char *fmt, va_list ap)
{
    if (buf == nullptr || size <= 0) {
        return -1;
    }
    int ret = vsnprintf_s(buf, size, size - 1, fmt, ap);
    return ret;
}

static bool StringAppendV(std::string& dst, const char* fmt, va_list ap) {
    char buffer[STRING_BUF_LEN] = {0};
    va_list bakAp;
    va_copy(bakAp, ap);
    int ret = BufferAppendV(buffer, sizeof(buffer), fmt, bakAp);
    va_end(bakAp);

    if (ret < static_cast<int>(sizeof(buffer))) {
        if (ret >= 0) {
            dst.append(buffer, ret);
            return true;
        } else {
            return false;
        }
    }

    int size = ret + 1;
    char* buf = new char[size];
    va_copy(bakAp, ap);
    ret = BufferAppendV(buf, size, fmt, bakAp);
    va_end(bakAp);

    if (ret >= 0 && ret < size) {
        dst.append(buf, ret);
    }
    delete[] buf;
    return true;
}

inline int BufferPrintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = BufferAppendV(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

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
