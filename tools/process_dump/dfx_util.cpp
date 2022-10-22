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
#include <fstream>
#include <iostream>
#include <securec.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_logger.h"

namespace OHOS {
namespace HiviewDFX {
int GetRealTargetPid()
{
    ProcInfo procInfo;
    (void)memset_s(&procInfo, sizeof(procInfo), 0, sizeof(struct ProcInfo));
    if (GetProcStatus(procInfo) == -1) {
        return -1;
    }

    return procInfo.ppid;
}

int GetProcStatus(struct ProcInfo& procInfo)
{
    char buf[STATUS_LINE_SIZE];
    FILE *fp = fopen(PROC_SELF_STATUS_PATH, "r");
    if (fp == nullptr) {
        return -1;
    }

    int p = 0, pp = 0, t = 0;
    while (!feof(fp)) {
        if (fgets(buf, STATUS_LINE_SIZE, fp) == nullptr) {
            fclose(fp);
            return -1;
        }

        if (strncmp(buf, PID_STR_NAME, strlen(PID_STR_NAME)) == 0) {
            // Pid:    1892
            if (sscanf_s(buf, "%*[^0-9]%d", &p) != 1) {
                DfxLogError("sscanf_s failed.");
            }
            procInfo.pid = p;
            if (procInfo.pid == getpid()) {
                procInfo.ns = false;
                procInfo.tid = gettid();
                procInfo.ppid = getppid();
                break;
            }
            procInfo.ns = true;
            continue;
        }

        if (strncmp(buf, PPID_STR_NAME, strlen(PPID_STR_NAME)) == 0) {
            // PPid:   240
            if (sscanf_s(buf, "%*[^0-9]%d", &pp) != 1) {
                DfxLogError("sscanf_s failed.");
            }
            procInfo.ppid = pp;
            continue;
        }

        // NSpid:  1892    1
        if (strncmp(buf, NSPID_STR_NAME, strlen(NSPID_STR_NAME)) == 0) {
            if (sscanf_s(buf, "%*[^0-9]%d%*[^0-9]%d", &p, &t) != 2) {
                DfxLogError("sscanf_s failed.");
            }
            procInfo.tid = t;
            break;
        }
    }
    (void)fclose(fp);
    return 0;
}

bool ReadStringFromFile(const std::string &path, std::string &buf, size_t len)
{
    if (len <= 1) {
        return false;
    }

    char realPath[PATH_MAX] = {0};
    if (!realpath(path.c_str(), realPath)) {
        return false;
    }

    std::ifstream file;
    file.open(realPath);
    if (!file.is_open()) {
        return false;
    }

    std::istreambuf_iterator<char> start(file), end;
    std::string str(start, end);
    buf = str.substr(0, len);
    file.close();
    return true;
}

bool TrimAndDupStr(const std::string &source, std::string &str)
{
    if (source.empty()) {
        return false;
    }

    const char *begin = source.data();
    const char *end = begin + source.size();
    if (begin == end) {
        DfxLogError("Source is empty");
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

std::string GetCurrentTimeStr(uint64_t current)
{
    time_t now = time(nullptr);
    uint64_t millsecond = 0;
    const uint64_t ratio = 1000;
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

int PrintFormat(char *buf, int size, const char *format, ...)
{
    int ret = -1;
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, size, size - 1, format, args);
    va_end(args);
    if (ret <= 0) {
        DfxLogError("snprintf_s failed.");
    }
    return ret;
}
}   // namespace HiviewDFX
}   // namespace OHOS
