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

/* This files is writer log to file module on process dump module. */

#include "test_utils.h"

#include <directory_ex.h>
#include <file_ex.h>
#include <gtest/gtest.h>
#include <hilog/log.h>
#include <string_ex.h>

namespace OHOS {
namespace HiviewDFX {
uint64_t GetSelfMemoryCount()
{
    std::string path = "/proc/self/smaps_rollup";
    std::string content;
    if (!OHOS::LoadStringFromFile(path, content)) {
        GTEST_LOG_(INFO) << "Failed to load path content:" << path << "\n";
        return 0;
    }

    std::vector<std::string> result;
    OHOS::SplitStr(content, "\n", result);
    std::string pss;
    for (auto const& str : result) {
        if (str.find("Pss:") != std::string::npos) {
            GTEST_LOG_(INFO) << "Find:" << str << "\n";
            pss = str;
            break;
        }
    }

    if (pss.empty()) {
        GTEST_LOG_(INFO) << "Failed to find Pss.\n";
        return 0;
    }

    uint64_t retVal = 0;
    for (size_t i = 0; i < pss.size(); i++) {
        if (isdigit(pss[i])) {
            retVal = atoi(&pss[i]);
            break;
        }
    }

    return retVal;
}

uint32_t GetSelfMapsCount()
{
    std::string path = "/proc/self/maps";
    std::string content;
    if (!OHOS::LoadStringFromFile(path, content)) {
        return 0;
    }

    std::vector<std::string> result;
    OHOS::SplitStr(content, "\n", result);
    return result.size();
}

uint32_t GetSelfFdCount()
{
    std::string path = "/proc/self/fd";
    std::vector<std::string> content;
    OHOS::GetDirFiles(path, content);
    return content.size();
}

bool CheckLogFileExist(int32_t pid, std::string& fileName)
{
    std::string path = "/data/log/faultlog/temp";
    std::vector<std::string> content;
    OHOS::GetDirFiles(path, content);
    for (auto const& file : content) {
        if (file.find(std::to_string(pid)) != std::string::npos) {
            fileName = file;
            return true;
        }
    }
    return false;
}

void CheckContent(const std::string& content, const std::string& keyContent, bool checkExist)
{
    bool findKeyContent = false;
    if (content.find(keyContent) != std::string::npos) {
        findKeyContent = true;
    }

    if (checkExist && !findKeyContent) {
        GTEST_LOG_(INFO) << "Failed to find " << keyContent;
        GTEST_LOG_(INFO) << " in " << content;
        FAIL();
    }

    if (!checkExist && findKeyContent) {
        GTEST_LOG_(INFO) << "Find " << keyContent;
        GTEST_LOG_(INFO) << " in " << content;
        FAIL();
    }
}
} // namespace HiviewDFX
} // namespace OHOS
