/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef DUMP_CATCHER_PIPE_DATA_H
#define DUMP_CATCHER_PIPE_DATA_H

#include <cstdint>
#include <string>

#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {

class DumpCatcherPipeData {
public:
    DumpCatcherPipeData(int32_t pid, int32_t bufPipe, int32_t resPipe);
    ~DumpCatcherPipeData();
    DumpCatcherPipeData(const DumpCatcherPipeData&) = delete;
    DumpCatcherPipeData& operator=(const DumpCatcherPipeData&) = delete;

    bool ReadBuf();
    bool ReadRes(int& pollRet);

    bool IsValid() const { return bufFd_ && resFd_; }
    int32_t GetPid() const { return pid_; }
    const SmartFd& GetBufFd() const { return bufFd_; }
    const SmartFd& GetResFd() const { return resFd_; }
    const std::string& GetBufMsg() const { return bufMsg_; }
    const std::string& GetResMsg() const { return resMsg_; }
    const std::string& GetMainThreadStackMsg() const { return mainThreadStackMsg_; }
    std::string& MutableBufMsg() { return bufMsg_; }
    std::string& MutableResMsg() { return resMsg_; }

private:
    void TryCompleteFirstPacket();

    int32_t pid_;
    SmartFd bufFd_;
    SmartFd resFd_;
    std::string bufMsg_;
    std::string resMsg_;
    std::string mainThreadStackMsg_;
    uint32_t mainThreadStackExpectLen_ = 0;
    uint32_t mainThreadStackCurLen_ = 0;
    bool isMainThreadStackPacket_ = false;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // DUMP_CATCHER_PIPE_DATA_H
