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

#include "proc_util.h"

#include <fcntl.h>
#include <string>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "securec.h"
#include "smart_fd.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "ProcUtil"
#endif

namespace OHOS {
namespace HiviewDFX {
bool Schedstat::ParseSchedstat(const std::string& schedstatPath)
{
    char realPath[PATH_MAX] = {0};
    if (realpath(schedstatPath.c_str(), realPath) == nullptr) {
        DFXLOGE("path invalid. %{public}s", schedstatPath.c_str());
        return false;
    }

    SmartFd fd(open(realPath, O_RDONLY));
    if (fd < 0) {
        DFXLOGE("open %{public}s failed. %{public}d", schedstatPath.c_str(), errno);
        return false;
    }

    std::string line;
    if (!ReadFdToString(fd, line)) {
        DFXLOGE("Schedstat::FromProc read failed.");
        return false;
    }

    constexpr int fieldCnt = 3;
    int parsed = sscanf_s(line.c_str(),
        "%" PRIu64 " "      // run time(1)
        "%" PRIu64 " "      // wait time(2)
        "%" PRIu64,         // timeslices(3)
        &runTime_, &waitTime_, &timeslicesNum_);

    if (parsed != fieldCnt) {
        DFXLOGE("Schedstat::FromProc parser failed.");
        Reset();
        return false;
    }

    return true;
}

std::string Schedstat::ToString() const
{
    // schedstat={178500, 54250, 3}
    return "schedstat={" +
        std::to_string(runTime_) + ", " +
        std::to_string(waitTime_) + ", " +
        std::to_string(timeslicesNum_) +
        "}";
}

void Schedstat::Reset()
{
    runTime_ = 0;
    waitTime_ = 0;
    timeslicesNum_ = 0;
}

bool ThreadInfo::ParserThreadInfo(pid_t tid)
{
    std::string schedstatPath = std::string(PROC_SELF_TASK_PATH) + "/" + std::to_string(tid) + "/schedstat";
    schedstat_.ParseSchedstat(schedstatPath);

    ProcessInfo info;
    if (!ParseProcInfo(tid, info)) {
        return false;
    }

    state_ = info.state;
    utime_ = info.utime;
    stime_ = info.stime;
    cutime_ = info.cutime;
    return true;
}

std::string ThreadStateToString(ThreadState state)
{
    switch (state) {
        case ThreadState::RUNNING:
            return "RUNNING";
        case ThreadState::SLEEP:
            return "SLEEP";
        case ThreadState::DISK_SLEEP:
            return "DISK_SLEEP";
        case ThreadState::ZOMBIE:
            return "ZOMBIE";
        case ThreadState::STOPPED:
            return "STOPPED";
        case ThreadState::TRACING_STOP:
            return "TRACING_STOP";
        case ThreadState::DEAD_2_6:
        case ThreadState::DEAD:
            return "DEAD";
        case ThreadState::WAKEKILL:
            return "WAKEKILL";
        case ThreadState::WAKING:
            return "WAKING";
        case ThreadState::PARKED:
            return "PARKED";
        case ThreadState::IDLE:
            return "IDLE";
        default:
            return "Unknown";
    }
}

std::string ThreadInfo::ToString() const
{
    // state=SLEEP, utime=1, stime=3, cutime=0, schedstat={178500, 54250, 3}
    return "state=" + ThreadStateToString(state_) + ", " +
        "utime=" + std::to_string(utime_) + ", " +
        "stime=" + std::to_string(stime_) + ", " +
        "cutime=" + std::to_string(cutime_) + ", " +
        schedstat_.ToString();
}

static bool ParseStatFromString(const std::string& statStr, ProcessInfo& info)
{
    size_t commStart = statStr.find_first_of("(");
    size_t commEnd = statStr.find_last_of(")");
    if (commStart == std::string::npos || commEnd == std::string::npos ||
        commStart >= commEnd || commEnd - commStart > TASK_COMM_LEN + 1) {
        DFXLOGE("parser comm error. %{public}zu", commEnd - commStart);
        return false;
    }

    // pid(1)
    int parsed = sscanf_s(statStr.c_str(), "%d (", &info.pid);
    if (parsed != 1) {
        DFXLOGE("parser pid failed.");
        return false;
    }

    // comm(2)
    std::string comm = statStr.substr(commStart + 1, commEnd - commStart - 1);
    (void)memcpy_s(info.comm, TASK_COMM_LEN, comm.c_str(), comm.length());
    parsed = sscanf_s(statStr.substr(commEnd).c_str(),
        ") %c "             // state(3)
        "%d "               // ppid(4)
        "%*d%*d%*d%*d"      // skip pgrp(5), session(6), tty(7), tpgid(8)
        "%*u%*u%*u%*u%*u"   // skip flags(9), minflt(10), cminflt(11), majflt(12), cmajflt(13)
        "%" PRIu64 " "      // utime(14)
        "%" PRIu64 " "      // stime(15)
        "%" PRIi64 " "      // cutime(16)
        "%" PRIi64 " "      // cstime(17)
        "%" PRIi64 " "      // priority(18)
        "%" PRIi64 " "      // nice(19)
        "%" PRIi64 " "      // num_threads(20)
        "%*ld "             // skip itrealcalue(21)
        "%" PRIu64 " "      // starttime(22)
        "%*lu "             // skip vsize(23)
        "%d "               // rss(24)
        "%*lu%*lu%*lu%*lu"  // skip rsslim(25), startcode(26), endcode(27), startstack(28)
        "%*lu%*lu"          // skip kstkesp(29), kstkeip(30)
        "%" PRIu64 " "      // signal(31)
        "%" PRIu64 " "      // blocked(32)
        "%" PRIu64 " "      // sigignore(33)
        "%" PRIu64,         // sigcatch(34)
        &info.state, 1, &info.ppid, &info.utime, &info.stime, &info.cutime, &info.cstime, &info.priority,
        &info.nice, &info.numThreads, &info.starttime, &info.rss, &info.signal, &info.blocked,
        &info.sigignore, &info.sigcatch
    );

    constexpr int fieldCnt = 15;
    if (parsed != fieldCnt) {
        DFXLOGE("parser field failed. cnt %{public}d", parsed);
        return false;
    }
    return true;
}

bool ParseStat(const std::string& statPath, ProcessInfo& info)
{
    char realPath[PATH_MAX] = {0};
    if (realpath(statPath.c_str(), realPath) == nullptr) {
        DFXLOGE("path invalid. %{public}s", statPath.c_str());
        return false;
    }

    SmartFd fd(open(realPath, O_RDONLY));
    if (fd < 0) {
        DFXLOGE("open %{public}s failed. %{public}d", statPath.c_str(), errno);
        return false;
    }

    std::string line;
    if (!ReadFdToString(fd, line)) {
        DFXLOGE("read file %{public}s failed.", statPath.c_str());
        return false;
    }

    return ParseStatFromString(line, info);
}

bool ParseProcInfo(pid_t pid, ProcessInfo& info)
{
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    return ParseStat(path, info);
}
} // namespace HiviewDFX
} // namespace OHOS
