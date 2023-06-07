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

#ifndef DFX_PROCINFO_H
#define DFX_PROCINFO_H

#include <cstdio>
#include <iostream>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "dfx_define.h"

#ifdef __cplusplus
extern "C" {
#endif
namespace OHOS {
namespace HiviewDFX {
bool GetProcStatus(struct ProcInfo& procInfo);
bool GetProcStatusByPid(int realPid, struct ProcInfo& procInfo);
bool TidToNstid(const int pid, const int tid, int& nstid);
bool IsThreadInCurPid(int32_t tid);
bool GetTidsByPidWithFunc(const int pid, std::vector<int>& tids, std::function<bool(int)> const& func);
bool GetTidsByPid(const int pid, std::vector<int>& tids, std::vector<int>& nstids);
void ReadThreadName(const int tid, std::string& str);
void ReadProcessName(const int pid, std::string& str);
void ReadProcessWchan(std::string& result, const int pid, bool withThreadName = false);
void ReadThreadWchan(std::string& result, const int tid, bool withThreadName = false);
} // nameapace HiviewDFX
} // nameapace OHOS
#ifdef __cplusplus
}
#endif
#endif
