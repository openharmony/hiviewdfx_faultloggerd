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

#include "dfx_buffer_writer.h"

#include <securec.h>
#ifndef is_ohos_lite
#include <string_ex.h>
#endif // !is_ohos_lite
#include <unistd.h>

#include "dfx_log.h"
#include "dfx_define.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
static const int32_t INVALID_FD = -1;

DfxBufferWriter &DfxBufferWriter::GetInstance()
{
    static DfxBufferWriter ins;
    return ins;
}

void DfxBufferWriter::WriteMsg(const std::string& msg)
{
    if (writeFunc_ == nullptr) {
        writeFunc_ = DfxBufferWriter::DefaultWrite;
    }
    constexpr size_t step = 1024 * 1024;
    for (size_t i = 0; i < msg.size(); i += step) {
        int length = (i + step) < msg.size() ? step : msg.size() - i;
        writeFunc_(bufFd_, msg.substr(i, length).c_str(), length);
    }
}

bool DfxBufferWriter::WriteDumpRes(int32_t dumpRes)
{
    if (resFd_ == -1) {
        return false;
    }
    ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(resFd_, &dumpRes, sizeof(dumpRes)));
    if (nwrite != sizeof(dumpRes)) {
        DFXLOGE("%{public}s write fail, err:%{public}d", __func__, errno);
        return false;
    }
    return true;
}

void DfxBufferWriter::WriteFormatMsg(const char *format, ...)
{
    char buf[LINE_BUF_SIZE] = {0};
    va_list args;
    va_start(args, format);
    int ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    va_end(args);
    if (ret < 0) {
        DFXLOGE("%{public}s :: vsnprintf_s failed, line: %{public}d.", __func__, __LINE__);
        return;
    }
    WriteMsg(std::string(buf));
}

void DfxBufferWriter::AppendBriefDumpInfo(const std::string& info)
{
    briefDumpInfo_.append(info);
}

void DfxBufferWriter::PrintBriefDumpInfo()
{
#ifndef is_ohos_lite
    if (briefDumpInfo_.empty()) {
        DFXLOGE("crash base info is empty");
    }
    std::vector<std::string> dumpInfoLines;

    OHOS::SplitStr(briefDumpInfo_, "\n", dumpInfoLines);
    for (const auto& dumpInfoLine : dumpInfoLines) {
        DFXLOGI("%{public}s", dumpInfoLine.c_str());
    }
#endif
}

void DfxBufferWriter::Finish()
{
    if (bufFd_ != INVALID_FD) {
        if (fsync(bufFd_) == -1) {
            DFXLOGW("Failed to fsync fd.");
        }
        CloseFd(bufFd_);
    }
    CloseFd(resFd_);
}

void DfxBufferWriter::SetWriteResFd(int32_t fd)
{
    if (fd < 0) {
        DFXLOGE("invalid res fd, failed to set fd.\n");
        return;
    }
    resFd_ = fd;
}

void DfxBufferWriter::SetWriteBufFd(int32_t fd)
{
    if (fd < 0) {
        DFXLOGE("invalid buffer fd, failed to set fd.\n");
        return;
    }
    bufFd_ = fd;
}

void DfxBufferWriter::SetWriteFunc(BufferWriteFunc func)
{
    writeFunc_ = func;
}

int DfxBufferWriter::DefaultWrite(int32_t fd, const char *buf, const int len)
{
    if (buf == nullptr) {
        return -1;
    }
    if (fd > 0) {
        return OHOS_TEMP_FAILURE_RETRY(write(fd, buf, len));
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
