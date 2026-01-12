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
#ifndef LPERF_EVENT_RECORD_H
#define LPERF_EVENT_RECORD_H

#include <cstdint>
#include <linux/perf_event.h>
#include <securec.h>
#include <string>
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
constexpr uint32_t RECORD_SIZE_LIMIT = 65535;
constexpr ssize_t ERRINFOLEN = 512;

struct LperfRecordSampleData {
    uint32_t pid = 0;
    uint32_t tid = 0;                       /* if PERF_SAMPLE_TID */
    uint64_t time = 0;                      /* if PERF_SAMPLE_TIME */
    uint64_t nr = 0;                        /* if PERF_SAMPLE_CALLCHAIN */
    uint64_t* ips = nullptr;                /* if PERF_SAMPLE_CALLCHAIN */
};

inline std::string StringFormat(const char* fmt, ...)
{
    va_list vargs;
    char buf[1024] = {0}; // 1024: buf size
    std::string format(fmt);
    va_start(vargs, fmt);
    if (vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format.c_str(), vargs) < 0) {
        va_end(vargs);
        return "";
    }

    va_end(vargs);
    return buf;
}

class LperfRecordSample {
public:
    LperfRecordSampleData data_ = {};
    LperfRecordSample() = default;

    static const char* GetName();
    uint32_t GetType() const;
    bool Init(uint8_t* data);
    void Clear();
protected:
    void InitHeader(const uint8_t* data);

private:
    struct perf_event_header header_ = {};
    uint64_t sampleType_ = PERF_SAMPLE_TID | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_TIME;
};

class LperfRecordFactory {
public:
    static LperfRecordSample& GetLperfRecord(uint32_t type, uint8_t* data);
    static void ClearData();

private:
    static LperfRecordSample record_;
};

template<typename T>
bool PopFromBinary(bool condition, uint8_t*& data, T& dest, uint64_t& size);

template<typename T1, typename T2>
bool PopFromBinary2(bool condition, uint8_t*& data, T1& dest1, T2& dest2, uint64_t& size);

bool SetPointerOffset(uint8_t*& data, uint64_t offset, uint64_t& size);

#define NO_RETVAL /* retval */
#ifndef CHECK_TRUE_AND_RET
#define CHECK_TRUE_AND_RET(condition, retval, fmt, ...)                                            \
    do {                                                                                           \
        if (!(condition)) [[unlikely]] {                                                           \
            std::string str = StringFormat(fmt, ##__VA_ARGS__);                                    \
            DFXLOGE("%{public}s", str.c_str());                                                    \
            return retval;                                                                         \
        }                                                                                          \
    } while (0)
#endif

#ifndef CHECK_ERR
#define CHECK_ERR(err, fmt, ...)                                                                   \
    do {                                                                                           \
        if (err < 0) [[unlikely]] {                                                                \
            char errInfo[ERRINFOLEN] = { 0 };                                                      \
            strerror_r(errno, errInfo, ERRINFOLEN);                                                \
            DFXLOGE("%{public}s, error: %{public}d, errInfo: %{public}s",                          \
                          StringFormat(fmt, ##__VA_ARGS__).c_str(), errno, errInfo);               \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif // LPERF_EVENT_RECORD_H