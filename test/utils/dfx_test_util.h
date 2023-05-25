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

#ifndef DFX_TEST_UTIL
#define DFX_TEST_UTIL

#include <string>

static const std::string ACCOUNTMGR_NAME = "accountmgr";
static const std::string FOUNDATION_NAME = "foundation";
static const std::string APPSPAWN_NAME = "appspawn";

#if defined(__arm__)
#define REGISTERS           "r0:","r1:","r2:","r3:","r4:","r5:","r6:",\
                            "r7:","r8:","r9:","r10:","fp:","ip:","sp:","lr:","pc:"
#define REGISTERS_NUM       16
#define REGISTER_FORMAT_LENGTH    8
#elif defined(__aarch64__)
#define REGISTERS           "x0:","x1:","x2:","x3:","x4:","x5:","x6:","x7:","x8:",\
                            "x9:","x10:","x11:","x12:","x13:","x14:","x15:","x16:",\
                            "x17:","x18:","x19:","x20:","x21:","x22:","x23:","x24:",\
                            "x25:","x26:","x27:","x28:","x29:","lr:","sp:","pc:"
#define REGISTERS_NUM       33
#define REGISTER_FORMAT_LENGTH    16
#endif

namespace OHOS {
namespace HiviewDFX {
enum CrasherType {
    CRASHER_C,
    CRASHER_CPP
};
std::string ExecuteCommands(const std::string& cmds);
int GetProcessPid(const std::string& processName);
int LaunchTestHap(const std::string& abilityName, const std::string& bundleName);
void StopTestHap(const std::string& bundleName);
void InstallTestHap(const std::string& hapName);
void UninstallTestHap(const std::string& bundleName);
int CountLines(const std::string& fileName);
bool CheckProcessComm(int pid, const std::string& name);
int CheckKeyWords(const std::string& filePath, std::string *keywords, int length, int minRegIdx);
bool CheckContent(const std::string& content, const std::string& keyContent, bool checkExist);
int GetKeywordsNum(const std::string& msg, std::string *keywords, int length);
std::string GetCppCrashFileName(const pid_t pid);
uint32_t GetSelfFdCount();
uint32_t GetSelfMapsCount();
uint64_t GetSelfMemoryCount();
} // namespace HiviewDFX
} // namespace OHOS
#endif // DFX_TEST_UTIL