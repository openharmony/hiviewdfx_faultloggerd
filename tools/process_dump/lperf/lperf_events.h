/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef LPERF_EVENTS_H
#define LPERF_EVENTS_H

#include <functional>
#include <string>
#include <vector>
#include <linux/perf_event.h>
#include <poll.h>

#include "lperf_event_record.h"

namespace OHOS {
namespace HiviewDFX {
class LperfEvents {
public:
    LperfEvents();
    ~LperfEvents();

    int PrepareRecord();
    int StartRecord();
    bool StopRecord();

    void SetTid(const std::vector<int>& tids);
    void SetTimeOut(int timeOut);
    void SetSampleFrequency(unsigned int frequency);

    using ProcessRecordCB = std::function<void(LperfRecordSample& record)>;
    void SetRecordCallBack(ProcessRecordCB recordCallBack);
    void Clear();

private:
    struct MmapFd {
        int fd = 0;
        perf_event_mmap_page* mmapPage = nullptr;
        uint8_t *buf = nullptr;
        size_t bufSize = 0;
        size_t dataSize = 0;

        perf_event_header header = {};
    };

    bool PrepareFdEvents();
    bool AddRecordThreads();
    bool GetHeaderFromMmap(MmapFd& mmap);
    void GetRecordFieldFromMmap(MmapFd& mmap, void* dest, size_t destSize, size_t pos, size_t size);
    void GetRecordFromMmap(MmapFd& mmap);
    void ReadRecordsFromMmaps();
    bool PerfEventsEnable(bool enable);
    bool RecordLoop();

    MmapFd lperfMmap_ = {};
    ProcessRecordCB recordCallBack_;

    int lperfFd_ = -1;
    int timeOut_ = 0;
    unsigned int pageSize_ = 4096;
    unsigned int mmapPages_ = 1024;
    unsigned int sampleFreq_ = 0;
    std::vector<int> tids_;
    std::vector<struct pollfd> pollFds_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // LPERF_EVENTS_H