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

#ifndef DFX_UTIL_H
#define DFX_UTIL_H

#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {

typedef int (*BufferWriteFunc) (int32_t fd, const char *buf, const size_t len);

#ifndef is_host
bool TrimAndDupStr(const std::string &source, std::string &str);
bool TrimAndDupStr(const char* buff, size_t buffSize, std::string &str,
    size_t maxLength = static_cast<size_t>(NAME_BUF_LEN));
uint64_t GetTimeMilliSeconds(void);
uint64_t GetAbsTimeMilliSeconds(void);
AT_SYMBOL_DEFAULT std::string GetCurrentTimeStr(uint64_t current = 0);
bool ReadDirFiles(const std::string& path, std::vector<std::string>& files);
void ParseSiValue(siginfo_t& si, uint64_t& endTime, int& tid);
bool IsBetaVersion();
bool IsDeveloperMode();
bool IsOversea();
#endif
std::string GetArkWebCorePathPrefix();
off_t GetFileSize(int fd);
bool ReadFdToString(int fd, std::string& content);
uintptr_t StripPac(uintptr_t inAddr, uintptr_t pacMask);
bool SafeStrtolCpp(const std::string& numStr, long& out, int base);
int WriteBuf(int fd, const char* buf, size_t len);
int WriteStringMsg(const int fd, const std::string& msg, BufferWriteFunc writeFunc = WriteBuf);
#if is_ohos && !is_mingw
size_t ReadProcMemByPid(const pid_t pid, const uint64_t addr, void* data, size_t size);
#endif
} // nameapace HiviewDFX
} // nameapace OHOS

// this will also used for libunwinder head (out of namespace)
#if defined(is_mingw) && is_mingw
#ifndef MMAP_FAILED
#define MMAP_FAILED reinterpret_cast<void *>(-1)
#endif
void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset);
int munmap(void *addr, size_t);
#endif
#endif
