/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "file_ex.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>

namespace OHOS {
static const int MAX_FILE_LENGTH = 32 * 1024 * 1024;
bool LoadStringFromFile(const std::string& filePath, std::string& content)
{
    std::ifstream file(filePath.c_str());
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const long fileLength = file.tellg();
    if (fileLength > MAX_FILE_LENGTH) {
        return false;
    }

    content.clear();
    file.seekg(0, std::ios::beg);
    std::copy(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), back_inserter(content));
    return true;
}
} // OHOS
