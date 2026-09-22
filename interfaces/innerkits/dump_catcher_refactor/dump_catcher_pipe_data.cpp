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

#include "dump_catcher_pipe_data.h"

#include <vector>

#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DumpCatcherPipe"
#endif

namespace OHOS {
namespace HiviewDFX {

DumpCatcherPipeData::DumpCatcherPipeData(int32_t pid, int32_t bufPipe, int32_t resPipe)
    : pid_(pid), bufFd_(bufPipe), resFd_(resPipe) {}

DumpCatcherPipeData::~DumpCatcherPipeData()
{
    RequestDelPipeFd(pid_);
}

void DumpCatcherPipeData::TryCompleteFirstPacket()
{
    if (!isMainThreadStackPacket_ || mainThreadStackCurLen_ != mainThreadStackExpectLen_) {
        return;
    }
    mainThreadStackMsg_ = std::move(bufMsg_);
    bufMsg_.clear();
    mainThreadStackCurLen_ = 0;
    mainThreadStackExpectLen_ = 0;
    isMainThreadStackPacket_ = false;
    DFXLOGI("First packet complete, saved to mainThreadStackMsg");
}

bool DumpCatcherPipeData::ReadBuf()
{
    std::vector<char> buffer(MAX_PIPE_SIZE, 0);
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(bufFd_.GetFd(), buffer.data(), MAX_PIPE_SIZE));
    if (nread <= 0) {
        DFXLOGW("ReadBuf :: read error");
        return false;
    }
    DFXLOGD("ReadBuf :: nread: %{public}zu", nread);
    bufMsg_.append(buffer.data(), static_cast<size_t>(nread));
    mainThreadStackCurLen_ += static_cast<uint32_t>(nread);
    TryCompleteFirstPacket();
    return true;
}

bool DumpCatcherPipeData::ReadRes(int& pollRet)
{
    DumpResMessage resMsg;
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(resFd_.GetFd(), &resMsg, sizeof(resMsg)));
    if (nread <= 0 || nread != sizeof(resMsg)) {
        DFXLOGW("ReadRes :: read error, nread=%{public}zd", nread);
        return false;
    }
    DFXLOGI("ReadRes: code=%{public}d, dataLen=%{public}u, currentLen=%{public}u",
        resMsg.code, resMsg.dataLen, mainThreadStackCurLen_);
    if (resMsg.code == DumpErrorCode::DUMP_EMAIN_THREAD_DONE) {
        mainThreadStackExpectLen_ = resMsg.dataLen;
        isMainThreadStackPacket_ = true;
        TryCompleteFirstPacket();
        if (isMainThreadStackPacket_) {
            DFXLOGI("First packet incomplete: %{public}u/%{public}u, will complete in ReadBuf",
                mainThreadStackCurLen_, mainThreadStackExpectLen_);
        }
        return false;
    }
    switch (resMsg.code) {
        case DUMP_ESUCCESS:
        case DUMP_THREAD_OVER_LIMIT:
            pollRet = DUMP_POLL_OK;
            break;
        case DUMP_ESYMBOL_NO_PARSE:
            pollRet = DUMP_POLL_NO_PARSE_SYMBOL;
            break;
        case DUMP_ESYMBOL_PARSE_TIMEOUT:
            pollRet = DUMP_POLL_PARSE_SYMBOL_TIMEOUT;
            break;
        default:
            pollRet = DUMP_POLL_RETURN;
            break;
    }
    resMsg_.append("Result: " + DfxDumpRes::ToString(resMsg.code) + "\n");
    return true;
}

} // namespace HiviewDFX
} // namespace OHOS
