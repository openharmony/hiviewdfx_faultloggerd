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
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
    int GetProcStatus(struct ProcInfo& procInfo);
    int GetProcStatusByPid(int realPid, struct ProcInfo& procInfo);
    int GetRealTargetPid();
    bool TidToNstid(const int pid, const int tid, int& nstid);
    bool ReadStringFromFile(const char *path, char *buf, size_t len);
    bool TrimAndDupStr(const std::string &source, std::string &str);
    void ReadThreadName(const int tid, std::string& str);
    void ReadProcessName(const int pid, std::string& str);
    uint64_t GetTimeMilliSeconds(void);
    std::string GetCurrentTimeStr(uint64_t current = 0);
    int PrintFormat(char *buf, int size, const char *format, ...);
} // nameapace HiviewDFX
} // nameapace OHOS

#endif // DFX_UTIL_H
