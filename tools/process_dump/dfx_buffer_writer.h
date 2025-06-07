/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#ifndef DFX_BUFFER_WRITER_H
#define DFX_BUFFER_WRITER_H

#include <cinttypes>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include "dfx_buffer_writer.h"
#include "nocopyable.h"

namespace OHOS {
namespace HiviewDFX {

typedef int (*BufferWriteFunc) (int32_t fd, const char *buf, const size_t len);

class DfxBufferWriter final {
public:
    static DfxBufferWriter &GetInstance();
    ~DfxBufferWriter() = default;
    void Finish();

    void SetWriteBufFd(int32_t fd);
    void SetWriteResFd(int32_t fd);
    void SetWriteFunc(BufferWriteFunc func);
    bool WriteDumpRes(int32_t dumpRes);

    void WriteMsg(const std::string& msg);
    void WriteFormatMsg(const char *format, ...);

    void AppendBriefDumpInfo(const std::string& info);
    void PrintBriefDumpInfo();
private:
    static int DefaultWrite(int32_t fd, const char *buf, const size_t len);

    DfxBufferWriter() = default;
    DISALLOW_COPY_AND_MOVE(DfxBufferWriter);

    BufferWriteFunc writeFunc_ = nullptr;
    int32_t bufFd_ = -1;
    int32_t resFd_ = -1;
    std::string briefDumpInfo_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
