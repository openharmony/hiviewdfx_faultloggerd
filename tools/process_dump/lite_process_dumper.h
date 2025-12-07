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

#ifndef LITE_PROCESS_DUMPER_H
#define LITE_PROCESS_DUMPER_H

#include <string>
#include <vector>

#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_dump_request.h"
#include "unwinder.h"
#include "decorative_dump_info.h"

namespace OHOS {
namespace HiviewDFX {
class MemoryNearRegisterUtil {
public:
    static MemoryNearRegisterUtil& GetInstance()
    {
        static MemoryNearRegisterUtil instance;
        return instance;
    }
    std::vector<MemoryBlockInfo> blocksInfo_;
};

class LiteProcessDumper {
public:
    void Dump(int uid);
private:
    bool ReadPipeData(int uid);
    void ReadRequest(int pipeReadFd);
    void ReadStat(int pipeReadFd);
    void ReadStatm(int pipeReadFd);
    void ReadStack(int pipeReadFd);
    void ReadMemoryNearRegister(int pipeReadFd, ProcessDumpRequest request);

    void InitProcess();
    void Unwind();

    void PrintAll();
    void PrintHeader();
    void PrintThreadInfo();
    void PrintRegisters();
    void PrintRegsNearMemory();
    void PrintFaultStack();
    void PrintMaps();
    void PrintOpenFiles();

    std::shared_ptr<DfxRegs> regs_;
    std::shared_ptr<DfxMaps> dfxMaps_;

    std::vector<uint8_t> stackBuf_;
    std::string stat_;
    std::string statm_;
    ProcessDumpRequest request_ {};
    std::string rawData_;
    std::shared_ptr<Unwinder> unwinder_;
    std::shared_ptr<DfxProcess> process_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
