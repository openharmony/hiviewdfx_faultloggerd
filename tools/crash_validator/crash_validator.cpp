/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "crash_validator.h"

#include <cinttypes>
#include <csignal>
#ifdef HISYSEVENT_ENABLE
#include <fcntl.h>
#include <hisysevent.h>
#include <securec.h>
#include <unistd.h>

#include "hisysevent_manager.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr char EVENT_CPP_CRASH[] = "CPP_CRASH";
constexpr char KEY_PROCESS_EXIT[] = "PROCESS_EXIT";
constexpr char KEY_NAME[] = "PROCESS_NAME";
constexpr char KEY_PID[] = "PID";
constexpr char KEY_UID[] = "UID";
constexpr char KEY_STATUS[] = "STATUS";
constexpr char KEY_LOG_PATH[] = "LOG_PATH";
constexpr char KEY_MODULE[] = "MODULE";
constexpr char INIT_LOG_PATTERN[] = "Service warning ";
constexpr char KEY_NO_LOG_EVENT_NAME[] = "CPP_CRASH_NO_LOG";
constexpr char KEY_HAPPEN_TIME[] = "HAPPEN_TIME";
constexpr int32_t LOG_SIZE = 1024;
constexpr uint64_t MAX_LOG_GENERATE_TIME = 600; // 600 seconds
constexpr int32_t KMSG_SIZE = 2049;
}
CrashValidator::CrashValidator() : stopReadKmsg_(false), totalEventCount_(0),
    normalEventCount_(0), kmsgReaderThread_(nullptr)
{
}

CrashValidator::~CrashValidator()
{
    if (kmsgReaderThread_ != nullptr) {
        kmsgReaderThread_ = nullptr;
    }
}

void CrashValidator::OnEvent(std::shared_ptr<HiviewDFX::HiSysEventRecord> sysEvent)
{
    std::lock_guard<std::mutex> lock(lock_);
    if (sysEvent == nullptr) {
        return;
    }
    auto domain = sysEvent->GetDomain();
    auto eventName = sysEvent->GetEventName();
    if (eventName == EVENT_CPP_CRASH) {
        HandleCppCrashEvent(sysEvent);
    } else if (eventName == KEY_PROCESS_EXIT) {
        HandleProcessExitEvent(sysEvent);
    }
}

void CrashValidator::OnServiceDied()
{
    printf("SysEventServiceDied?.\n");
}

bool CrashValidator::InitSysEventListener()
{
    std::vector<ListenerRule> sysRules;
    sysRules.emplace_back("RELIABILITY", "CPP_CRASH");
    sysRules.emplace_back("STARTUP", "PROCESS_EXIT");
    if (HiSysEventManager::AddListener(shared_from_this(), sysRules) != 0) {
        return false;
    }

    kmsgReaderThread_ = std::make_unique<std::thread>([this] {
        ReadServiceCrashStatus();
    });
    kmsgReaderThread_->detach();
    return true;
}

void CrashValidator::RemoveSysEventListener()
{
    int32_t result = HiSysEventManager::RemoveListener(shared_from_this());
    printf("remove listener result: %d\n", result);
}

void CrashValidator::PrintEvents(int fd, const std::vector<CrashEvent>& events, bool isMatched)
{
    std::vector<CrashEvent>::const_iterator it = events.begin();
    while (it != events.end()) {
        if (isMatched) {
            dprintf(fd, "Module:%s Time:%" PRIu64 " Pid:%" PRIu64 " Uid:%" PRIu64 "\n",
                it->name.c_str(),
                static_cast<uint64_t>(it->time),
                static_cast<uint64_t>(it->pid),
                static_cast<uint64_t>(it->uid));
        } else {
            dprintf(fd, "Module:%s Time:%" PRIu64 " Pid:%" PRIu64 " Uid:%" PRIu64 " HasLog:%d\n",
                it->name.c_str(),
                static_cast<uint64_t>(it->time),
                static_cast<uint64_t>(it->pid),
                static_cast<uint64_t>(it->uid),
                it->isCppCrash);
        }
        ++it;
    }
}

void CrashValidator::Dump(int fd)
{
    dprintf(fd, "Summary:\n");
    dprintf(fd, "Total Signaled Process:%d\n", totalEventCount_);
    dprintf(fd, "Total CppCrash Count:%d\n", normalEventCount_);
    if (totalEventCount_ > 0) {
        dprintf(fd, "CppCrash detect rate:%d%%\n",
            (normalEventCount_ * 100) / totalEventCount_); // 100 : percent ratio
    }

    std::lock_guard<std::mutex> lock(lock_);
    if (!noLogEvents_.empty()) {
        dprintf(fd, "No CppCrash Log Events(%zu):\n", noLogEvents_.size());
        PrintEvents(fd, noLogEvents_, false);
    }

    if (!pendingEvents_.empty()) {
        dprintf(fd, "Pending CppCrash Log Events(%zu):\n", pendingEvents_.size());
        PrintEvents(fd, pendingEvents_, false);
    }

    if (!matchedEvents_.empty()) {
        dprintf(fd, "Matched Events(%zu):\n", matchedEvents_.size());
        PrintEvents(fd, matchedEvents_, true);
    }
}

bool CrashValidator::RemoveSimilarEvent(const CrashEvent& event)
{
    for (const auto& matchedEvent : matchedEvents_) {
        if (matchedEvent.pid == event.pid && matchedEvent.uid == event.uid) {
            return true;
        }
    }
    std::vector<CrashEvent>::iterator it = pendingEvents_.begin();
    while (it != pendingEvents_.end()) {
        if (it->uid == event.uid && it->pid == event.pid) {
            if (it->isCppCrash != event.isCppCrash) {
                it = pendingEvents_.erase(it);
                normalEventCount_++;
                matchedEvents_.push_back(event);
            }
            return true;
        } else {
            ++it;
        }
    }
    return false;
}

void CrashValidator::HandleCppCrashEvent(std::shared_ptr<HiviewDFX::HiSysEventRecord> sysEvent)
{
    if (sysEvent == nullptr) {
        return;
    }
    CrashEvent crashEvent;
    crashEvent.isCppCrash = true;
    sysEvent->GetParamValue(KEY_HAPPEN_TIME, crashEvent.time);
    sysEvent->GetParamValue(KEY_UID, crashEvent.uid);
    sysEvent->GetParamValue(KEY_PID, crashEvent.pid);
    sysEvent->GetParamValue(KEY_LOG_PATH, crashEvent.path);
    sysEvent->GetParamValue(KEY_MODULE, crashEvent.name);
    printf("CPPCRASH:[Pid:%" PRIi64 " Uid:%" PRIi64 " Module:%s]\n",
        crashEvent.pid, crashEvent.uid, crashEvent.name.c_str());
    if (!RemoveSimilarEvent(crashEvent)) {
        totalEventCount_++;
        pendingEvents_.push_back(crashEvent);
    }
}

void CrashValidator::HandleProcessExitEvent(std::shared_ptr<HiviewDFX::HiSysEventRecord> sysEvent)
{
    if (sysEvent == nullptr) {
        return;
    }
    int64_t status64 = 0;
    sysEvent->GetParamValue(KEY_STATUS, status64);
    int32_t status = static_cast<int32_t>(status64);
    if (!WIFSIGNALED(status) && !WIFEXITED(status)) {
        return;
    }

    CrashEvent crashEvent;
    crashEvent.isCppCrash = false;
    crashEvent.time = sysEvent->GetTime();
    sysEvent->GetParamValue(KEY_UID, crashEvent.uid);
    sysEvent->GetParamValue(KEY_PID, crashEvent.pid);
    sysEvent->GetParamValue(KEY_NAME, crashEvent.name);
    int exitSigno = WTERMSIG(status);
    // crash in pid namespace exit signo is zero, instead of use exit status code.
    if (exitSigno == 0) {
        exitSigno = WEXITSTATUS(status);
    }

    int interestedSignalList[] = {
        SIGABRT, SIGBUS, SIGFPE, SIGILL,
        SIGSEGV, SIGSTKFLT, SIGSYS, SIGTRAP };
    bool shouldGenerateCppCrash = false;
    for (size_t i = 0; i < sizeof(interestedSignalList) / sizeof(interestedSignalList[0]); i++) {
        if (interestedSignalList[i] == exitSigno) {
            shouldGenerateCppCrash = true;
            break;
        }
    }

    if (!shouldGenerateCppCrash) {
        return;
    }

    printf("Process Exit Name:%s Time:%llu [Pid:%" PRIi64 " Uid:%" PRIi64 "] status:%d\n",
        crashEvent.name.c_str(),
        static_cast<unsigned long long>(crashEvent.time),
        crashEvent.pid,
        crashEvent.uid,
        status);
    if (!RemoveSimilarEvent(crashEvent)) {
        totalEventCount_++;
        pendingEvents_.push_back(crashEvent);
    }
}

void CrashValidator::CheckOutOfDateEvents()
{
    std::vector<CrashEvent>::iterator it = pendingEvents_.begin();
    while (it != pendingEvents_.end()) {
        uint64_t now = time(nullptr);
        uint64_t eventTime = it->time;
        if (eventTime > now) {
            eventTime = eventTime / 1000; // 1000 : convert to second
        }

        if (now > eventTime && now - eventTime < MAX_LOG_GENERATE_TIME) {
            ++it;
            continue;
        }

        if (!it->isCppCrash) {
            HiSysEventWrite(HiSysEvent::Domain::RELIABILITY, KEY_NO_LOG_EVENT_NAME, HiSysEvent::EventType::FAULT,
                KEY_NAME, it->name,
                KEY_PID, it->pid,
                KEY_UID, it->uid,
                KEY_HAPPEN_TIME, it->time);
            noLogEvents_.push_back(*it);
        } else {
            totalEventCount_++;
            normalEventCount_++;
        }
        it = pendingEvents_.erase(it);
    }
}

void CrashValidator::ReadServiceCrashStatus()
{
    char kmsg[KMSG_SIZE];
    int fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        printf("Failed to open /dev/kmsg.\n");
        return;
    }
    lseek(fd, 0, 3); // 3 : SEEK_DATA
    while (true) {
        ssize_t len;
        if (((len = read(fd, kmsg, sizeof(kmsg))) == -1) && errno == EPIPE) {
            continue;
        }
        if (len == -1 && errno == EINVAL) {
            printf("Failed to read kmsg\n");
            close(fd);
            return;
        }
        if (len < 1) {
            continue;
        }
        kmsg[len] = 0;
        if (stopReadKmsg_) {
            break;
        }
        std::string line = kmsg;
        auto pos = line.find(INIT_LOG_PATTERN);
        if (pos == std::string::npos) {
            continue;
        }
        std::string formattedLog = line.substr(pos + strlen(INIT_LOG_PATTERN));
        char name[LOG_SIZE] {0};
        int pid;
        int uid;
        int status;
        int ret = sscanf_s(formattedLog.c_str(), "%[^,], SIGCHLD received, pid:%d uid:%d status:%d.",
            name, sizeof(name), &pid, &uid, &status);
        if (ret <= 0) {
            printf("Failed to parse kmsg:%s", formattedLog.c_str());
            continue;
        }

        printf("Kernel:%s", formattedLog.c_str());
        HiSysEventWrite(HiSysEvent::Domain::STARTUP, KEY_PROCESS_EXIT, HiSysEvent::EventType::BEHAVIOR,
            KEY_NAME, name, KEY_PID, pid, KEY_UID, uid, KEY_STATUS, status);
    }
    close(fd);
}

bool CrashValidator::ValidateLogContent(const CrashEvent& event)
{
    // check later
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
