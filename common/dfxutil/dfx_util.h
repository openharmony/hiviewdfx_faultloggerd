/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_UTIL_H
#define DFX_UTIL_H

#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
AT_SYMBOL_HIDDEN bool TrimAndDupStr(const std::string &source, std::string &str);
AT_SYMBOL_HIDDEN uint64_t GetTimeMilliSeconds(void);
AT_SYMBOL_DEFAULT std::string GetCurrentTimeStr(uint64_t current = 0);
AT_SYMBOL_HIDDEN bool ReadDirFiles(const std::string& path, std::vector<std::string>& files);
AT_SYMBOL_HIDDEN bool ReadDirFilesByPid(const int& pid, std::vector<std::string>& files);
AT_SYMBOL_HIDDEN bool VerifyFilePath(const std::string& filePath, const std::vector<const std::string>& validPaths);
AT_SYMBOL_HIDDEN off_t GetFileSize(const int& fd);
AT_SYMBOL_HIDDEN int GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
