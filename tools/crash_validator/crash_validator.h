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
#ifndef FAULTLOGGERD_CRASH_VALIDATOR_H
#define FAULTLOGGERD_CRASH_VALIDATOR_H

#include <memory>
#include <mutex>
#include <vector>
#include <thread>

#include "hisysevent_listener.h"

namespace OHOS {
namespace HiviewDFX {
class CrashEvent {
public:
    uint64_t time {0};
    int64_t pid {0};
    int64_t uid {0};
    std::string path {""};
    std::string name {""};
    bool isCppCrash {false};
};
// every CPP_CRASH should have corresponding PROCESS_EXIT event and
// every PROCESS_EXIT event that triggered by crash signal should have corresponding CPP_CRASH events
// check the existence of these event to judge whether we have loss some crash log
class CrashValidator : public OHOS::HiviewDFX::HiSysEventListener,
    public std::enable_shared_from_this<CrashValidator> {
public:
    CrashValidator();
    ~CrashValidator();

    void OnEvent(std::shared_ptr<HiviewDFX::HiSysEventRecord> sysEvent) override;
    void OnServiceDied() override;

    bool InitSysEventListener();
    void RemoveSysEventListener();

    void Dump(int fd);

private:
    bool ValidateLogContent(const CrashEvent& event);
    bool RemoveSimilarEvent(const CrashEvent& event);
    void HandleCppCrashEvent(std::shared_ptr<HiviewDFX::HiSysEventRecord> sysEvent);
    void HandleProcessExitEvent(std::shared_ptr<HiviewDFX::HiSysEventRecord> sysEvent);
    void PrintEvents(int fd, const std::vector<CrashEvent>& events, bool isMatched);
    void ReadServiceCrashStatus();
    void CheckOutOfDateEvents();
    bool stopReadKmsg_;
    uint32_t totalEventCount_;
    uint32_t normalEventCount_;
    std::unique_ptr<std::thread> kmsgReaderThread_;
    std::vector<CrashEvent> pendingEvents_;
    std::vector<CrashEvent> noLogEvents_;
    std::vector<CrashEvent> matchedEvents_;
    std::mutex lock_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULTLOGGERD_CRASH_VALIDATOR_H