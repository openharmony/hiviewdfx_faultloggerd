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
#ifndef is_host
AT_SYMBOL_HIDDEN bool TrimAndDupStr(const std::string &source, std::string &str);
AT_SYMBOL_HIDDEN uint64_t GetTimeMilliSeconds(void);
AT_SYMBOL_HIDDEN uint64_t GetAbsTimeMilliSeconds(void);
AT_SYMBOL_DEFAULT std::string GetCurrentTimeStr(uint64_t current = 0);
AT_SYMBOL_HIDDEN bool ReadDirFiles(const std::string& path, std::vector<std::string>& files);
AT_SYMBOL_HIDDEN bool VerifyFilePath(const std::string& filePath, const std::vector<const std::string>& validPaths);
AT_SYMBOL_HIDDEN void ParseSiValue(siginfo_t& si, uint64_t& endTime, int& tid);
#endif
AT_SYMBOL_HIDDEN off_t GetFileSize(const int& fd);
AT_SYMBOL_HIDDEN bool ReadFdToString(int fd, std::string& content);
AT_SYMBOL_HIDDEN void CloseFd(int& fd);
AT_SYMBOL_HIDDEN uintptr_t StripPac(uintptr_t inAddr, uintptr_t pacMask);
AT_SYMBOL_HIDDEN bool IsBetaVersion();
AT_SYMBOL_HIDDEN bool IsDeveloperMode();
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
