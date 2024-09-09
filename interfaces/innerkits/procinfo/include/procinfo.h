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
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
/**
 * @brief  process information
*/
typedef struct ProcInfo {
    /** process id */
    int pid;
    /** parent process id */
    int ppid;
    /** namespace process id */
    int nsPid;
    /** namespace is enabled or not */
    bool ns = false;
} ProcInfo;

/**
 * @brief Get process status
 *
 * @param procInfo structure containing information about process(output parameter)
 * @return if succeed return true, otherwise return false
*/
bool GetProcStatus(struct ProcInfo& procInfo);
/**
 * @brief Get process status by specified process id
 *
 * @param realPid real process id
 * @param procInfo structure containing information about process(output parameter)
 * @return if succeed return true, otherwise return false
*/
bool GetProcStatusByPid(int realPid, struct ProcInfo& procInfo);
/**
 * @brief convert real tid to namespace tid
 *
 * @param pid process id
 * @param tid thread id
 * @param nsTid namespace tid(output parameter)
 * @return if succeed return true, otherwise return false
*/
bool TidToNstid(const int pid, const int tid, int& nstid);
/**
 * @brief convert real tid to namespace tid
 *
 * @param pid process id
 * @param tid thread id
 * @return if succeed return true, otherwise return false
*/
bool IsThreadInPid(int32_t pid, int32_t tid);
/**
 * @brief Get thread id by process id and function
 *
 * @param pid process id
 * @param tids thread ids(output parameter)
 * @param func function
 * @return if succeed return true, otherwise return false
*/
bool GetTidsByPidWithFunc(const int pid, std::vector<int>& tids, std::function<bool(int)> const& func);
/**
 * @brief Get thread ids and namespace thread ids by process id
 *
 * @param pid process id
 * @param tids thread ids(output parameter)
 * @param nsTid namespace tids(output parameter)
 * @return if succeed return true, otherwise return false
*/
bool GetTidsByPid(const int pid, std::vector<int>& tids, std::vector<int>& nstids);
/**
 * @brief read thread name by id
 * @param tid thread id
 * @param str thread name
*/
void ReadThreadName(const int tid, std::string& str);
/**
 * @brief read thread name by id
 * @param tid thread id
 * @param str thread name
*/
void ReadThreadNameByPidAndTid(const int pid, const int tid, std::string& str);
/**
 * @brief read process name by id
 * @param pid process id
 * @param str process name
*/
void ReadProcessName(const int pid, std::string& str);

/**
 * @brief read process status by id
 * @param result content of status
 * @param pid process id
 * @param withThreadName whether output thread name or not
*/
void ReadProcessStatus(std::string& result, const int pid);

/**
 * @brief read process wchan by id
 * @param result content of wchan
 * @param pid process id
 * @param onlyPid if only print process wchan
 * @param withThreadName whether output thread name or not
*/
void ReadProcessWchan(std::string& result, const int pid, bool onlyPid = false, bool withThreadName = false);

/**
 * @brief read thread wchan by id
 * @param result content of wchan
 * @param tid thread id
 * @param withThreadName whether output thread name or not
*/
void ReadThreadWchan(std::string& result, const int tid, bool withThreadName = false);
/**
 * @brief Get stacktrace head info
*/
std::string GetStacktraceHeader();
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
