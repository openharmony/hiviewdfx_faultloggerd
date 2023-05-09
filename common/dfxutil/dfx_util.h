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
    AT_SYMBOL_HIDDEN int GetProcStatus(struct ProcInfo& procInfo);
    AT_SYMBOL_HIDDEN int GetProcStatusByPid(int realPid, struct ProcInfo& procInfo);
    AT_SYMBOL_HIDDEN int GetRealTargetPid();
    AT_SYMBOL_HIDDEN bool TidToNstid(const int pid, const int tid, int& nstid);
    AT_SYMBOL_HIDDEN bool ReadStringFromFile(const char *path, char *buf, size_t len);
    AT_SYMBOL_HIDDEN bool TrimAndDupStr(const std::string &source, std::string &str);
    AT_SYMBOL_HIDDEN void ReadThreadName(const int tid, std::string& str);
    AT_SYMBOL_HIDDEN void ReadProcessName(const int pid, std::string& str);
    AT_SYMBOL_HIDDEN uint64_t GetTimeMilliSeconds(void);
    AT_SYMBOL_HIDDEN std::string GetCurrentTimeStr(uint64_t current = 0);
    AT_SYMBOL_HIDDEN bool ReadDirFiles(const std::string& path, std::vector<std::string>& files);
    AT_SYMBOL_HIDDEN bool ReadDirFilesByPid(const int& pid, std::vector<std::string>& files);
    AT_SYMBOL_HIDDEN size_t GetFileSize(const int& fd);
} // nameapace HiviewDFX
} // nameapace OHOS

#endif // DFX_UTIL_H
