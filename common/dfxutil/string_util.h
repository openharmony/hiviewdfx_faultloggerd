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

/* This files contains header of util module. */

#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <cstdint>
#include <string>
#include <securec.h>

namespace OHOS {
namespace HiviewDFX {
namespace {
const int DFX_STRING_BUF_LEN = 4096;
const std::string EMPTY_STRING = "";
}

inline bool StartsWith(const std::string& s, const std::string& prefix)
{
    return s.substr(0, prefix.size()) == prefix;
}

inline bool EndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
            s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
}

inline void Trim(std::string& str)
{
    std::string blanks("\f\v\r\t\n ");
    str.erase(0, str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + sizeof(char));
}

template<typename... VA>
std::string StringPrintf(const char *fmt, VA... args)
{
    if (fmt == nullptr) {
        return EMPTY_STRING;
    }

    char buf[DFX_STRING_BUF_LEN] = {0};
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, fmt, args...) < 0) {
        return EMPTY_STRING;
    }
    return std::string(buf);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
