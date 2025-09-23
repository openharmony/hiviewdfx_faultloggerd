/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef PROC_UTIL_H
#define PROC_UTIL_H

#include <string>

namespace OHOS {
namespace HiviewDFX {

class Schedstat {
public:
    bool ParseSchedstat(const std::string& schedstatPath);
    std::string ToString() const;
    void Reset();

    uint64_t runTime_{0}; // time spent on the cpu (in nanoseconds)
    uint64_t waitTime_{0}; // time spent waiting on a runqueue (in nanoseconds)
    uint64_t timeslicesNum_{0}; // of timeslices run on this cpu
};

enum class ThreadState : char {
    UNKNOWM = 0,
    RUNNING = 'R', //Runnig
    SLEEP = 'S', // Sleep in an interruptible wait
    DISK_SLEEP = 'D', // Waiting in uninterruptible disk sleep
    ZOMBIE = 'Z', // Zombie
    STOPPED = 'T', // Stopped (on a signal) or (before Linux2.6.33) trace stopped
    TRACING_STOP = 't', // Tracing stop (Linux 2.6.33 onward)
    DEAD_2_6 = 'X', // Dead (from Linux 2.6.0 oneard)
    DEAD = 'x', // Dead (Linux 2.6.33 to 3.13 only)
    WAKEKILL = 'K', // Wakekill (Linux 2.6.33 to 3.13 only)
    WAKING = 'W', // Wakekill (Linux 2.6.33 to 3.13 only)
    PARKED = 'P', // Parked (Linux 2.6.33 to 3.13 only)
    IDLE = 'I', // Idle (Linux 4.14 onward)
};

class ThreadInfo {
public:
    bool ParserThreadInfo(pid_t tid);
    std::string ToString() const;

    ThreadState state_{ThreadState::UNKNOWM};
    uint64_t utime_{0};
    uint64_t stime_{0};
    int64_t cutime_{0};
    Schedstat schedstat_;
};

constexpr int TASK_COMM_LEN = 16;

struct ProcessInfo {
    int32_t pid{0}; // field 1
    char comm[TASK_COMM_LEN + 1]{0}; // field 2
    ThreadState state{ThreadState::UNKNOWM}; // field 3
    int32_t ppid{0}; // field 4
    uint64_t utime{0}; // field 14
    uint64_t stime{0}; // field 15
    int64_t cutime{0}; // field 16
    int64_t cstime{0}; // field 17
    int64_t priority{0}; // field 18
    int64_t nice{0}; // field 19
    int64_t numThreads{0}; // field 20
    uint64_t starttime{0}; // field 22
    int32_t rss{0}; // field 24
    uint64_t signal{0}; // field 31
    uint64_t blocked{0}; // field 32
    uint64_t sigignore{0}; // field 33
    uint64_t sigcatch{0}; // field 34
};

std::string ThreadStateToString(ThreadState state);
bool ParseStat(const std::string& statPath, ProcessInfo& info);
bool ParseProcInfo(pid_t pid, ProcessInfo& info);
bool ParseThreadInfo(pid_t tid, ProcessInfo& info);
std::string GetFirstNumberSeq(const std::string& cont);
bool GetUidAndSigBlk(pid_t pid, long& uid, uint64_t& sigBlk);
bool IsSigDumpMask(uint64_t sigBlk);
} // namespace Dfx
} // namespace OHOS
#endif // PROC_UTIL_H
