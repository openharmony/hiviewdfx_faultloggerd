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
#include "decorative_dump_info.h"
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_util.h"
#include "dfx_process.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "unwinder.h"
namespace OHOS {
namespace HiviewDFX {
REGISTER_DUMP_INFO_CLASS(ExtraCrashInfo);

void ExtraCrashInfo::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    DfxBufferWriter::GetInstance().WriteMsg(GetCrashObjContent(request));
}

std::string ExtraCrashInfo::ReadCrashObjMemory(pid_t tid, uintptr_t addr, size_t length)
{
    DFXLOGI("Start read memory type of crashObj, memory length(%zu).", length);
    constexpr size_t step = sizeof(uintptr_t);
    std::string memoryContent = StringPrintf("ExtraCrashInfo(Memory start address %018" PRIx64 "):",
        static_cast<uint64_t>(addr));
    size_t size = (length + step - 1) / step;
    std::vector<uintptr_t> memory(size, 0);
    if (ReadProcMemByPid(tid, addr, memory.data(), length) != length) {
        DFXLOGE("[%{public}d]: read target mem error %{public}s", __LINE__, strerror(errno));
        memoryContent += "\n";
        return memoryContent;
    }
    for (size_t index = 0; index < size; index++) {
        size_t offset = index * step;
        if (offset % 0x20 == 0) {  // Print offset every 32 bytes
            memoryContent += StringPrintf("\n+0x%03" PRIx64 ":", static_cast<uint64_t>(offset));
        }
        memoryContent += StringPrintf(" %018" PRIx64, static_cast<uint64_t>(memory[index]));
    }
    memoryContent += "\n";
    return memoryContent;
}

std::string ExtraCrashInfo::GetCrashObjContent(const ProcessDumpRequest& request)
{
    std::string content = "";
#ifdef __LP64__
    if (request.type != ProcessDumpType::DUMP_TYPE_CPP_CRASH || request.crashObj == 0) {
        DFXLOGI("crash obj not int.");
        return content;
    }
    uintptr_t type = request.crashObj >> 56; // 56 :: Move 56 bit to the right
    uintptr_t addr = request.crashObj & 0xffffffffffffff;
    std::vector<size_t> memorylengthTable = {0, 64, 256, 1024, 2048, 4096};
    if (type != 0 && type < memorylengthTable.size()) {
        content = ReadCrashObjMemory(request.nsPid, addr, memorylengthTable[type]);
    }
#endif
    return content;
}
}
}