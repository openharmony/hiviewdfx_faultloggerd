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
    bool Dump(int pid);
private:
    bool ReadPipeData(int pid);
    bool ReadRequest(int pipeReadFd);
    void SetProcessdumpTimeout(siginfo_t &si);
    void FormatJsonInfoIfNeed();
    bool ReadStat(int pipeReadFd);
    bool ReadStatm(int pipeReadFd);
    bool ReadStack(int pipeReadFd);
    bool ReadOtherThreadStack(int pipeReadFd);
    bool LoopReadPipe(int pipeReadFd, void* buf, size_t length);
    MemoryBlockInfo ReadSingleRegMem(int pipeReadFd, uintptr_t nameAddr, unsigned int count, unsigned int forward);
    bool ReadMemoryNearRegister(int pipeReadFd, ProcessDumpRequest request);

    void InitProcess();
    void Unwind();
    void UnwindOtherThread();

    void PrintAll();
    void PrintHeader();
    void PrintThreadInfo();
    void PrintOtherThreadInfo();
    void PrintRegisters();
    void PrintRegsNearMemory();
    void PrintFaultStack();
    void PrintMaps();
    void CollectOpenFiles();
    void PrintOpenFiles();
    void MmapJitSymbol();
    void MunmapJitSymbol();
    bool Report();

    std::shared_ptr<DfxRegs> regs_;
    std::shared_ptr<DfxMaps> dfxMaps_;

    std::vector<uint8_t> stackBuf_;
    std::vector<std::vector<uint8_t>> otherThreadStackBuf_;
    std::string stat_;
    std::string statm_;
    ProcessDumpRequest request_ {};
    std::vector<ThreadDumpRequest> otherThreadRequest_;
    std::string rawData_;
    std::shared_ptr<Unwinder> unwinder_;
    std::vector<std::shared_ptr<Unwinder>> otherThreadUnwinder_;
    std::shared_ptr<DfxProcess> process_;
    std::string keyThreadStackStr_;
    std::map<int, std::string> fdFiles_;
    void* jitSymbolMMap_ = MAP_FAILED;
    bool isJsonDump_ = false;
    uint64_t expectedDumpFinishTime_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
