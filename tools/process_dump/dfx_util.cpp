/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains process dump common tool functions. */

#include "dfx_util.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
bool ReadStringFromFile(const std::string &path, std::string &buf, size_t len)
{
    DfxLogDebug("Enter %s.", __func__);
    if (len <= 1) {
        return false;
    }

    char realPath[PATH_MAX] = {0};
    if (!realpath(path.c_str(), realPath)) {
        return false;
    }

    std::ifstream file;
    file.open(realPath);
    if (!file.is_open()) {
        return false;
    }

    std::istreambuf_iterator<char> start(file), end;
    std::string str(start, end);
    buf = str.substr(0, len);
    file.close();
    DfxLogDebug("Exit %s.", __func__);
    return true;
}

bool TrimAndDupStr(const std::string &source, std::string &str)
{
    DfxLogDebug("Enter %s.", __func__);
    if (source.empty()) {
        return false;
    }

    const char *begin = source.data();
    const char *end = begin + source.size();
    if (begin == end) {
        DfxLogInfo("Source is empty");
        return false;
    }

    while ((begin < end) && isspace(*begin)) {
        begin++;
    }

    while ((begin < end) && isspace(*(end - 1))) {
        end--;
    }

    if (begin == end) {
        return false;
    }

    uint32_t maxStrLen = NAME_LEN;
    uint32_t offset = end - begin;
    if (maxStrLen > offset) {
        maxStrLen = offset;
    }

    str.assign(begin, maxStrLen);
    DfxLogDebug("Exit %s.", __func__);
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS
