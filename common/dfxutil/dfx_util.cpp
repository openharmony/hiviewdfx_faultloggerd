/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <securec.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include "dfx_define.h"
#include "dfx_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxUtil"
#endif

namespace OHOS {
namespace HiviewDFX {
static const char PPID_STR_NAME[] = "PPid:";
static const char NSPID_STR_NAME[] = "NSpid:";
static const int ARGS_COUNT_ONE = 1;
static const int ARGS_COUNT_TWO = 2;
static const int STATUS_LINE_SIZE = 1024;

static int GetProcStatusByPath(struct ProcInfo& procInfo, const std::string& path)
{
    char buf[STATUS_LINE_SIZE];
    FILE *fp = fopen(path.c_str(), "r");
    if (fp == nullptr) {
        LOGE("%s fopen %s failed", __func__, path.c_str());
        return -1;
    }

    int pid = 0;
    int ppid = 0;
    int nsPid = 0;
    while (!feof(fp)) {
        if (fgets(buf, STATUS_LINE_SIZE, fp) == nullptr) {
            fclose(fp);
            return -1;
        }

        if (strncmp(buf, PPID_STR_NAME, strlen(PPID_STR_NAME)) == 0) {
            // PPid:   240
            if (sscanf_s(buf, "%*[^0-9]%d", &ppid) != ARGS_COUNT_ONE) {
                LOGW("%s sscanf_s failed", __func__);
            }
            procInfo.ppid = ppid;
            continue;
        }

        // NSpid:  1892    1
        if (strncmp(buf, NSPID_STR_NAME, strlen(NSPID_STR_NAME)) == 0) {
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
    return 0;
}

bool TidToNstid(const int pid, const int tid, int& nstid)
{
    char path[NAME_LEN];
    (void)memset_s(path, sizeof(path), '\0', sizeof(path));
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/task/%d/status", pid, tid) <= 0) {
        LOGW("%s snprintf_s failed", __func__);
        return false;
    }

    struct ProcInfo procInfo;
    if (GetProcStatusByPath(procInfo, path) < 0) {
        return false;
    }
    nstid = procInfo.nsPid;
    return true;
}

int GetProcStatusByPid(int realPid, struct ProcInfo& procInfo)
{
    std::string path = "/proc/" + std::to_string(realPid) + "/status";
    return GetProcStatusByPath(procInfo, path);
}

int GetProcStatus(struct ProcInfo& procInfo)
{
    return GetProcStatusByPath(procInfo, PROC_SELF_STATUS_PATH);
}

int GetRealTargetPid()
{
    ProcInfo procInfo;
    (void)memset_s(&procInfo, sizeof(procInfo), 0, sizeof(struct ProcInfo));
    if (GetProcStatus(procInfo) == -1) {
        return -1;
    }
    return procInfo.ppid;
}

bool IsThreadInCurPid(int32_t tid)
{
    std::string path = std::string(PROC_SELF_TASK_PATH) + "/" + std::to_string(tid);
    return access(path.c_str(), F_OK) == 0;
}

bool GetTidsByPid(const int pid, std::vector<int>& tids, std::vector<int>& nstids)
{
    struct ProcInfo procInfo;
    (void)GetProcStatusByPid(pid, procInfo);

    std::vector<std::string> files;
    if (ReadDirFilesByPid(pid, files)) {
        for (size_t i = 0; i < files.size(); ++i) {
            pid_t tid = atoi(files[i].c_str());
            if (tid == 0) {
                continue;
            }

            tids.push_back(tid);

            pid_t nstid = tid;
            if (procInfo.ns) {
                TidToNstid(pid, tid, nstid);
                nstids.push_back(nstid);
            }
        }
    }
    if (!procInfo.ns) {
        nstids = tids;
    }
    return (tids.size() > 0);
}

bool ReadStringFromFile(const char *path, char *buf, size_t len)
{
    if ((len <= 1) || (buf == nullptr) || (path == nullptr)) {
        return false;
    }

    char realPath[PATH_MAX] = {0};
    if (realpath(path, realPath) == nullptr) {
        return false;
    }

    FILE *fp = fopen(realPath, "r");
    if (fp == nullptr) {
        LOGE("%s fopen %s failed", __func__, realPath);
        return false;
    }

    char *ptr = buf;
    for (size_t i = 0; i < len; i++) {
        int c = getc(fp);
        if (c == EOF) {
            *ptr++ = 0x00;
            break;
        } else {
            *ptr++ = c;
        }
    }
    fclose(fp);
    return false;
}

bool TrimAndDupStr(const std::string &source, std::string &str)
{
    if (source.empty()) {
        return false;
    }

    const char *begin = source.data();
    const char *end = begin + source.size();
    if (begin == end) {
        DFXLOG_ERROR("Source is empty");
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
    uint32_t offset = static_cast<uint32_t>(end - begin);
    if (maxStrLen > offset) {
        maxStrLen = offset;
    }

    str.assign(begin, maxStrLen);
    return true;
}

void ReadThreadName(const int tid, std::string& str)
{
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/comm", tid) <= 0) {
        return;
    }

    char buf[NAME_LEN];
    ReadStringFromFile(path, buf, NAME_LEN);
    TrimAndDupStr(std::string(buf), str);
}

void ReadProcessName(const int pid, std::string& str)
{
    char path[NAME_LEN] = "\0";
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/cmdline", pid) <= 0) {
        return;
    }

    char buf[NAME_LEN];
    ReadStringFromFile(path, buf, NAME_LEN);
    TrimAndDupStr(std::string(buf), str);
}

uint64_t GetTimeMilliSeconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * NUMBER_ONE_THOUSAND) + // 1000 : second to millisecond convert ratio
        (((uint64_t)ts.tv_nsec) / NUMBER_ONE_MILLION); // 1000000 : nanosecond to millisecond convert ratio
}

std::string GetCurrentTimeStr(uint64_t current)
{
    time_t now = time(nullptr);
    uint64_t millsecond = 0;
    const uint64_t ratio = NUMBER_ONE_THOUSAND;
    if (current > static_cast<uint64_t>(now)) {
        millsecond = current % ratio;
        now = static_cast<time_t>(current / ratio);
    }

    auto tm = std::localtime(&now);
    char seconds[128] = {0}; // 128 : time buffer size
    if (tm == nullptr || strftime(seconds, sizeof(seconds) - 1, "%Y-%m-%d %H:%M:%S", tm) == 0) {
        return "invalid timestamp\n";
    }

    char millBuf[256] = {0}; // 256 : milliseconds buffer size
    int ret = snprintf_s(millBuf, sizeof(millBuf), sizeof(millBuf) - 1,
        "%s.%03u\n", seconds, millsecond);
    if (ret <= 0) {
        return "invalid timestamp\n";
    }
    return std::string(millBuf, strlen(millBuf));
}

bool ReadDirFiles(const std::string& path, std::vector<std::string>& files)
{
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return false;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        // current dir OR parent dir
        if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0)) {
            continue;
        }
        files.emplace_back(std::string(ent->d_name));
    }
    (void)closedir(dir);
    return (files.size() > 0);
}

bool ReadDirFilesByPid(const int& pid, std::vector<std::string>& files)
{
    char path[PATH_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/task", pid) <= 0) {
        return false;
    }

    char realPath[PATH_MAX];
    if (!realpath(path, realPath)) {
        return false;
    }
    return ReadDirFiles(realPath, files);
}

bool VerifyFilePath(const std::string& filePath, const std::vector<const std::string>& validPaths)
{
    if (validPaths.size() == 0) {
        return true;
    }

    for (auto validPath : validPaths) {
        if (filePath.find(validPath) == 0) {
            return true;
        }
    }
    return false;
}

off_t GetFileSize(const int& fd)
{
    off_t fileSize = 0;
    if (fd >= 0) {
        struct stat fileStat;
        if (fstat(fd, &fileStat) == 0) {
            fileSize = fileStat.st_size;
        }
    }
    return fileSize;
}
}   // namespace HiviewDFX
}   // namespace OHOS
