/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "dfx_util.h"

#if defined(is_mingw) && is_mingw
#include <memoryapi.h>
#include <windows.h>
#endif
#ifndef is_host
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <securec.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include <sys/stat.h>

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
#ifndef is_host
bool TrimAndDupStr(const std::string &source, std::string &str)
{
    if (source.empty()) {
        return false;
    }

    const char *begin = source.data();
    const char *end = begin + source.size();
    if (begin == end) {
        DFXLOG_ERROR("%s", "Source is empty");
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

    uint32_t maxStrLen = NAME_BUF_LEN;
    uint32_t offset = static_cast<uint32_t>(end - begin);
    if (maxStrLen > offset) {
        maxStrLen = offset;
    }

    str.assign(begin, maxStrLen);
    return true;
}

uint64_t GetTimeMilliSeconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * NUMBER_ONE_THOUSAND) + // 1000 : second to millisecond convert ratio
        (((uint64_t)ts.tv_nsec) / NUMBER_ONE_MILLION); // 1000000 : nanosecond to millisecond convert ratio
}

uint64_t GetAbsTimeMilliSeconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * NUMBER_ONE_THOUSAND) +
        (static_cast<uint64_t>(ts.tv_nsec) / NUMBER_ONE_MILLION);
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

void ParseSiValue(siginfo_t& si, uint64_t& endTime, int& tid)
{
    const int flagOffset = 63;
    if ((reinterpret_cast<uint64_t>(si.si_value.sival_ptr) & (1ULL << flagOffset)) != 0) {
        endTime = reinterpret_cast<uint64_t>(si.si_value.sival_ptr) & (~(1ULL << flagOffset));
        tid = 0;
    } else {
        endTime = 0;
        tid = si.si_value.sival_int;
    }
}
#endif

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

bool ReadFdToString(int fd, std::string& content)
{
    content.clear();
    struct stat sb;
    if (fstat(fd, &sb) != -1 && sb.st_size > 0) {
        content.reserve(sb.st_size);
    }

    char buf[BUFSIZ] __attribute__((__uninitialized__));
    ssize_t n;
    while ((n = OHOS_TEMP_FAILURE_RETRY(read(fd, &buf[0], sizeof(buf)))) > 0) {
        content.append(buf, n);
    }
    return (n == 0);
}
}   // namespace HiviewDFX
}   // namespace OHOS

// this will also used for libunwinder head (out of namespace)
#if defined(is_mingw) && is_mingw
std::string GetLastErrorString()
{
    LPVOID lpMsgBuf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), 0, (LPTSTR)&lpMsgBuf, 0, NULL);
    std::string error((LPTSTR)lpMsgBuf);
    LocalFree(lpMsgBuf);
    return error;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset)
{
    HANDLE FileHandle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (FileHandle == INVALID_HANDLE_VALUE) {
        return MMAP_FAILED;
    }

    LOGD("fd is %d", fd);

    HANDLE FileMappingHandle = ::CreateFileMappingW(FileHandle, 0, PAGE_READONLY, 0, 0, 0);
    if (FileMappingHandle == nullptr) {
        LOGE("CreateFileMappingW %zu Failed with %ld:%s", length, GetLastError(), GetLastErrorString().c_str());
        return MMAP_FAILED;
    }

    void *mapAddr = ::MapViewOfFile(FileMappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (mapAddr == nullptr) {
        LOGE("MapViewOfFile %zu Failed with %ld:%s", length, GetLastError(), GetLastErrorString().c_str());
        return MMAP_FAILED;
    }

    // Close all the handles except for the view. It will keep the other handles
    // alive.
    ::CloseHandle(FileMappingHandle);
    return mapAddr;
}

int munmap(void *addr, size_t)
{
    /*
        On success, munmap() returns 0.  On failure, it returns -1, and
        errno is set to indicate the error (probably to EINVAL).

        UnmapViewOfFile function (memoryapi.h)

        If the function succeeds, the return value is nonzero.
        If the function fails, the return value is zero. To get extended error information, call
    GetLastError.
    */
    return !UnmapViewOfFile(addr);
}
#endif
