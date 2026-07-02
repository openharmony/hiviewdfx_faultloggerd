/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "file_util.h"

namespace OHOS {
namespace HiviewDFX {
constexpr int MAX_FILE_LENGTH = 32 * 1024 * 1024;

bool LoadStringFromFile(const std::string& filePath, std::string& content)
{
    FILE* file = fopen(filePath.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }

    content.clear();
    char buffer[4096];
    size_t totalBytes = 0;

    while (true) {
        size_t readSize = fread(buffer, 1, sizeof(buffer), file);
        if (readSize > 0) {
            if (totalBytes + readSize > static_cast<size_t>(MAX_FILE_LENGTH)) {
                fclose(file);
                content.clear();
                return false;
            }
            content.append(buffer, readSize);
            totalBytes += readSize;
        }

        if (readSize < sizeof(buffer)) {
            break;
        }
    }

    bool hasError = (ferror(file) != 0);
    (void)fclose(file);

    if (hasError) {
        content.clear();
        return false;
    }

    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
