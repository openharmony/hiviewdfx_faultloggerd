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
#ifndef DFX_TEST_UTIL_H
#define DFX_TEST_UTIL_H

#include <string>
#include <vector>
#include <list>
#include <csignal>
#include <regex>
#include <sys/wait.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <ctime>

#define NOINLINE __attribute__((noinline))

#define POWERMGR_NAME             "powermgr"
#define FOUNDATION_NAME           "foundation"
#define APPSPAWN_NAME             "appspawn"
#define TEST_BUNDLE_NAME          "com.example.myapplication"
#define TRUNCATE_TEST_BUNDLE_NAME "e.myapplication"


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
#elif defined(__x86_64__)
#define REGISTERS           "rax:","rdx:","rcx:","rbx:","rsi:","rdi:","rbp:","rsp:",\
                            "r8:","r9:","r10:","r11:","r12:","r13:","r14:","r15:","rip:"
#define REGISTERS_NUM       17
#define REGISTER_FORMAT_LENGTH    16
#endif
const char* const TEMP_DIR = "/data/log/faultlog/temp/";

namespace OHOS {
namespace HiviewDFX {
class TestScopedPidReaper {
public:
    explicit TestScopedPidReaper(pid_t pid) : pid_(pid) {}
    ~TestScopedPidReaper() { Kill(pid_); }

    static void Kill(pid_t pid)
    {
        if (pid <= 0) {
            return;
        }
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }
private:
    pid_t pid_;
};

enum CrasherType {
    CRASHER_C,
    CRASHER_CPP
};

struct LineRule {
    std::string regString;
    std::regex lineReg;
    size_t needMatchCnt{1};

    explicit LineRule(std::string reg, size_t cnt = 1) : regString(reg), lineReg(std::regex(reg)), needMatchCnt(cnt) {}
};

std::string ExecuteCommands(const std::string& cmds);
bool ExecuteCommands(const std::string& cmds, std::vector<std::string>& ress);
int GetProcessPid(const std::string& processName);
int LaunchTestHap(const std::string& abilityName, const std::string& bundleName);
void StopTestHap(const std::string& bundleName);
void InstallTestHap(const std::string& hapName);
void UninstallTestHap(const std::string& bundleName);
int CountLines(const std::string& fileName);
bool CheckProcessComm(int pid, const std::string& name);
int CheckKeyWords(const std::string& filePath, std::string *keywords, int length, int minRegIdx);
int CheckKeyWords(const std::string& filePath, std::vector<std::string>& keywords, int minRegIdx);
bool CheckContent(const std::string& content, const std::string& keyContent, bool checkExist);
int GetKeywordsNum(const std::string& msg, std::string *keywords, int length);
int GetKeywordCount(const std::string& msg, const std::string& keyword);
std::string GetDumpLogFileName(const std::string& prefix, const pid_t pid, const std::string& tempPath);
std::string GetCppCrashFileName(const pid_t pid, const std::string& tempPath = TEMP_DIR);
uint32_t GetSelfFdCount();
uint32_t GetSelfMapsCount();
uint64_t GetSelfMemoryCount();
void CheckResourceUsage(uint32_t fdCount, uint32_t mapsCount, uint64_t memCount);
std::string WaitCreateCrashFile(const std::string& prefix, pid_t pid, int retryCnt = 3);
std::string WaitCreateFile(const std::string& folder, std::regex& reg, time_t timeOut = 10);
bool CreatePipeFd(int (&fd)[2]);
void ClosePipeFd(int& fd);
void NotifyProcStart(int (&fd)[2]);
void WaitProcStart(int (&fd)[2]);
void CheckAndExit(bool hasFailure);
bool IsLinuxKernel();
bool CheckLineMatch(const std::string& filePath, std::list<LineRule>& rules);

template<typename T>
class AsyncWorker {
public:
    explicit AsyncWorker(std::function<void(T&)> dataClean)
        : thread_(), result_(), ready_(false), resultClean_(dataClean) {}
    ~AsyncWorker()
    {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // 启动异步任务（只能调用一次）
    void Start(std::function<T(void)> func)
    {
        if (!started_.test_and_set()) {
            thread_ = std::thread([this, func] { this->Task(func); });
        }
    }

    // 等待并获取结果
    T GetResult(std::chrono::seconds timeOut)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeOut, [this] { return ready_; })) {
            resultClean_(result_);
        }
        return result_;
    }

private:
    void Task(std::function<T(void)> func)
    {
        T res = func();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = res;
            ready_ = true;
        }
        cv_.notify_one();
    }

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    T result_;
    bool ready_;
    std::atomic_flag started_ = ATOMIC_FLAG_INIT;
    std::function<void(T&)> resultClean_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // DFX_TEST_UTIL
