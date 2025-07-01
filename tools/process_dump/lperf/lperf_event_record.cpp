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

#include "lperf_event_record.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxLperfRecordSample"

const char* const PERF_RECORD_TYPE_SAMPLE = "sample";
}
LperfRecordSample LperfRecordFactory::record_ = {};

const char* LperfRecordSample::GetName()
{
    return PERF_RECORD_TYPE_SAMPLE;
}

uint32_t LperfRecordSample::GetType() const
{
    return header_.type;
}

void LperfRecordSample::InitHeader(uint8_t* data)
{
    if (data == nullptr) {
        header_.type = PERF_RECORD_MMAP;
        header_.misc = PERF_RECORD_MISC_USER;
        header_.size = 0;
        return;
    }
    header_ = *(reinterpret_cast<perf_event_header *>(data));
}

bool LperfRecordSample::Init(uint8_t* data)
{
    InitHeader(data);
    CHECK_TRUE_AND_RET(data != nullptr, false, "LperfRecordSample Init error");
    data_ = {};
    uint64_t dataSize = static_cast<uint64_t>(RECORD_SIZE_LIMIT);
    CHECK_TRUE_AND_RET(SetPointerOffset(data, sizeof(header_), dataSize), false, "set header_ offset error");

    // parse record according SAMPLE_TYPE
    bool popId = PopFromBinary2<uint32_t, uint32_t>(sampleType_ & PERF_SAMPLE_TID,
                                                    data, data_.pid, data_.tid, dataSize);
    CHECK_TRUE_AND_RET(popId, false, "Init PERF_SAMPLE_TID error");
    CHECK_TRUE_AND_RET(PopFromBinary<uint64_t>(sampleType_ & PERF_SAMPLE_TIME, data, data_.time, dataSize),
                       false, "Init PERF_SAMPLE_TIME error");
    CHECK_TRUE_AND_RET(PopFromBinary<uint64_t>(sampleType_ & PERF_SAMPLE_CALLCHAIN, data, data_.nr, dataSize),
                       false, "Init PERF_SAMPLE_CALLCHAIN error");
    if (data_.nr > 0) {
        // the pointer is from input(data), require caller keep input(data) with *this together
        // think it in next time
        data_.ips = reinterpret_cast<uint64_t *>(data);
        CHECK_TRUE_AND_RET(SetPointerOffset(data, data_.nr * sizeof(uint64_t), dataSize),
                           false, "set ips offset error");
    }
    return true;
}

void LperfRecordSample::Clear()
{
    data_.pid = 0;
    data_.tid = 0;
    data_.time = 0;
    data_.nr = 0;
    data_.ips = nullptr;
}

LperfRecordSample& LperfRecordFactory::GetLperfRecord(uint32_t type, uint8_t* data)
{
    if (type != PERF_RECORD_SAMPLE || !record_.Init(data)) {
        record_.Clear();
        DFXLOGE("Init LperfRecordSample data error");
    }
    return record_;
}

void LperfRecordFactory::ClearData()
{
    record_.Clear();
}

template<typename T>
inline bool PopFromBinary(bool condition, uint8_t*& data, T& dest, uint64_t& size)
{
    CHECK_TRUE_AND_RET(sizeof(T) <= size, false, "PopFromBinary error");
    if (condition) {
        dest = *(reinterpret_cast<const T *>(data));
        data += sizeof(T);
        size -= sizeof(T);
    }
    return true;
}

template<typename T1, typename T2>
inline bool PopFromBinary2(bool condition, uint8_t*& data, T1& dest1, T2& dest2, uint64_t& size)
{
    CHECK_TRUE_AND_RET(sizeof(T1) + sizeof(T2) <= size, false, "PopFromBinary2 error");
    if (condition) {
        dest1 = *(reinterpret_cast<const T1 *>(data));
        data += sizeof(T1);
        dest2 = *(reinterpret_cast<const T2 *>(data));
        data += sizeof(T2);
        size -= (sizeof(T1) + sizeof(T2));
    }
    return true;
}

inline bool SetPointerOffset(uint8_t*& data, uint64_t offset, uint64_t& size)
{
    CHECK_TRUE_AND_RET(offset <= size && offset <= RECORD_SIZE_LIMIT, false, "SetPointerOffset error");
    size -= offset;
    data += offset;
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS