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

#include "procinfo.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <securec.h>
#include <fcntl.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_util.h"
#include "file_util.h"
#include "string_printf.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
const char * const PID_STR_NAME = "Pid:";
const char * const PPID_STR_NAME = "PPid:";
const char * const NSPID_STR_NAME = "NSpid:";
const int ARGS_COUNT_ONE = 1;
const int ARGS_COUNT_TWO = 2;
const int STATUS_LINE_SIZE = 1024;
}

static bool GetProcStatusByPath(struct ProcInfo& procInfo, const std::string& path)
{
    char buf[STATUS_LINE_SIZE];
    FILE *fp = fopen(path.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }

    int pid = 0;
    int ppid = 0;
    int nsPid = 0;
    while (!feof(fp)) {
        if (fgets(buf, STATUS_LINE_SIZE, fp) == nullptr) {
            fclose(fp);
            return false;
        }

        if (strncmp(buf, PID_STR_NAME, strlen(PID_STR_NAME)) == 0) {
            // Pid:   1892
            if (sscanf_s(buf, "%*[^0-9]%d", &pid) != ARGS_COUNT_ONE) {
                procInfo.pid = getpid();
            } else {
                procInfo.pid = pid;
            }
            procInfo.nsPid = pid;
            continue;
        }

        if (strncmp(buf, PPID_STR_NAME, strlen(PPID_STR_NAME)) == 0) {
            // PPid:   240
            if (sscanf_s(buf, "%*[^0-9]%d", &ppid) != ARGS_COUNT_ONE) {
                procInfo.ppid = getppid();
            } else {
                procInfo.ppid = ppid;
            }
            continue;
        }

        if (strncmp(buf, NSPID_STR_NAME, strlen(NSPID_STR_NAME)) == 0) {
            // NSpid:  1892    1
            if (sscanf_s(buf, "%*[^0-9]%d%*[^0-9]%d", &pid, &nsPid) != ARGS_COUNT_TWO) {
                procInfo.ns = false;
                procInfo.nsPid = pid;
            } else {
                procInfo.ns = true;
                procInfo.nsPid = nsPid;
            }
            procInfo.pid = pid;
            break;
        }
    }
    (void)fclose(fp);
    return true;
}

static void MoveMainThreadToHead(const int pid, std::vector<std::string>& tids)
{
    auto it = std::find(tids.begin(), tids.end(), std::to_string(pid));
    if (it != tids.end()) {
        std::swap(*it, tids.front());
    }
}

bool TidToNstid(const int pid, const int tid, int& nstid)
{
    std::string path = StringPrintf("/proc/%d/task/%d/status", pid, tid);
    if (path.empty()) {
        return false;
    }

    struct ProcInfo procInfo;
    if (!GetProcStatusByPath(procInfo, path)) {
        return false;
    }
    nstid = procInfo.nsPid;
    return true;
}

bool GetProcStatusByPid(int realPid, struct ProcInfo& procInfo)
{
    if (realPid == getpid()) {
        return GetProcStatus(procInfo);
    }
    std::string path = StringPrintf("/proc/%d/status", realPid);
    return GetProcStatusByPath(procInfo, path);
}

bool GetProcStatus(struct ProcInfo& procInfo)
{
    return GetProcStatusByPath(procInfo, PROC_SELF_STATUS_PATH);
}

bool IsThreadInPid(int32_t pid, int32_t tid)
{
    std::string path;
    if (pid == getpid()) {
        path = StringPrintf("%s/%d", PROC_SELF_TASK_PATH, tid);
    } else {
        path = StringPrintf("/proc/%d/task/%d", pid, tid);
    }
    return access(path.c_str(), F_OK) == 0;
}

bool GetTidsByPidWithFunc(const int pid, std::vector<int>& tids, std::function<bool(int)> const& func)
{
    std::string path;
    if (pid == getpid()) {
        path = std::string(PROC_SELF_TASK_PATH);
    } else {
        path = StringPrintf("/proc/%d/task", pid);
    }

    std::vector<std::string> files;
    if (ReadDirFiles(path, files)) {
        MoveMainThreadToHead(pid, files);
        for (size_t i = 0; i < files.size(); ++i) {
            pid_t tid = atoi(files[i].c_str());
            if (tid == 0) {
                continue;
            }
            tids.emplace_back(tid);

            if (func != nullptr) {
                func(tid);
            }
        }
    }
    return (tids.size() > 0);
}

bool GetTidsByPid(const int pid, std::vector<int>& tids, std::vector<int>& nstids)
{
    struct ProcInfo procInfo;
    (void)GetProcStatusByPid(pid, procInfo);

    std::function<bool(int)> func = nullptr;
    if (procInfo.ns) {
        func = [&](int tid) {
            pid_t nstid = tid;
            TidToNstid(pid, tid, nstid);
            nstids.emplace_back(nstid);
            return true;
        };
    }
    bool ret = GetTidsByPidWithFunc(pid, tids, func);
    if (ret && !procInfo.ns) {
        nstids = tids;
    }
    return (nstids.size() > 0);
}

std::string GetStacktraceHeader()
{
    pid_t pid = getpid();
    std::string ss = "\nTimestamp:" + GetCurrentTimeStr() + "Pid:" + std::to_string(pid) + "\n" +
        "Uid:" + std::to_string(getuid()) + "\n";
    std::string processName;
    ReadProcessName(pid, processName);
    std::string processNameNoNul;
    for (char c : processName) {
        if (c != '\0') {
            processNameNoNul += c;
        } else {
            break;
        }
    }
    ss += "Process name::" + processNameNoNul + "\n";
    return ss;
}

void ReadThreadName(const int tid, std::string& str)
{
    std::string path = StringPrintf("/proc/%d/comm", tid);
    std::string name;
    OHOS::HiviewDFX::LoadStringFromFile(path, name);
    TrimAndDupStr(name, str);
}

void ReadThreadNameByPidAndTid(const int pid, const int tid, std::string& str)
{
    std::string path = StringPrintf("/proc/%d/task/%d/comm", pid, tid);
    std::string name;
    OHOS::HiviewDFX::LoadStringFromFile(path, name);
    TrimAndDupStr(name, str);
}

void ReadProcessName(const int pid, std::string& str)
{
    std::string path;
    if (pid == getpid()) {
        path = std::string(PROC_SELF_CMDLINE_PATH);
    } else {
        path = StringPrintf("/proc/%d/cmdline", pid);
    }
    std::string name;
    OHOS::HiviewDFX::LoadStringFromFile(path, name);
    TrimAndDupStr(name, str);
}

void ReadProcessStatus(std::string& result, const int pid)
{
    std::string path = StringPrintf("/proc/%d/status", pid);
    if (access(path.c_str(), F_OK) != 0) {
        result.append(StringPrintf("Failed to access path(%s), errno(%d).\n", path.c_str(), errno));
        return;
    }
    std::string content;
    OHOS::HiviewDFX::LoadStringFromFile(path, content);
    if (!content.empty()) {
        std::string str = StringPrintf("Process status:\n%s\n", content.c_str());
        result.append(str);
    }
}

void ReadProcessWchan(std::string& result, const int pid, bool onlyPid, bool withThreadName)
{
    std::string path = StringPrintf("/proc/%d/wchan", pid);
    if (access(path.c_str(), F_OK) != 0) {
        result.append(StringPrintf("Failed to access path(%s), errno(%d).\n", path.c_str(), errno));
        return;
    }
    std::string ss;
    std::string content;
    OHOS::HiviewDFX::LoadStringFromFile(path, content);
    if (!content.empty()) {
        ss = StringPrintf("Process wchan:\n%s\n", content.c_str());
    }
    if (onlyPid) {
        result.append(ss);
        return;
    }
    ss += "\nProcess threads wchan:\n";
    ss += "=======================================\n";
    bool flag = false;
    std::string comm = "";
    std::string wchan = "";
    std::string taskPath = StringPrintf("/proc/%d/task", pid);
    std::vector<std::string> files;
    flag = ReadDirFiles(taskPath, files);
    for (size_t i = 0; i < files.size(); ++i) {
        std::string tidStr = files[i];
        std::string commPath = StringPrintf("%s/%s/comm", taskPath.c_str(), tidStr.c_str());
        std::string wchanPath = StringPrintf("%s/%s/wchan", taskPath.c_str(), tidStr.c_str());
        OHOS::HiviewDFX::LoadStringFromFile(commPath, comm);
        OHOS::HiviewDFX::LoadStringFromFile(wchanPath, wchan);
        if (!comm.empty() && !wchan.empty()) {
            flag = true;
            if (withThreadName) {
                ss += "Tid:" + tidStr + ", Name:" + comm;
            }
            ss += "wchan:" + wchan + "\n";
        }
    }

    if (!flag) {
        ss += "Failed to access path: " + taskPath + "\n";
    }
    ss += "=======================================\n";
    result.append(ss);
}

void ReadThreadWchan(std::string& result, const int tid, bool withThreadName)
{
    std::string ss;
    if (withThreadName) {
        std::string threadName;
        ReadThreadName(tid, threadName);
        ss = "Tid:" + std::to_string(tid) + ", Name:" + threadName + "\n";
    }
    std::string wchanPath = StringPrintf("%s/%d/wchan", PROC_SELF_TASK_PATH, tid);
    std::string wchan;
    if (OHOS::HiviewDFX::LoadStringFromFile(wchanPath, wchan)) {
        ss += "wchan:" + wchan + "\n";
    } else {
        ss += "Load thread wchan failed.\n";
    }
    result = ss;
}
}   // namespace HiviewDFX
}   // namespace OHOS
