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

#ifndef MINIDUMP_DUMPER_H
#define MINIDUMP_DUMPER_H
#ifndef is_ohos_lite
#include <string>
#include <vector>
#include <sys/types.h>

#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_dump_request.h"
#include "unwinder.h"
#include "decorative_dump_info.h"
#include "minidump_parser.h"
namespace OHOS {
namespace HiviewDFX {
class MinidumpDumper {
public:
    bool Dump(int pid, int pipeFd, bool enableMinidump, bool enableMinidumpToCrashLog);
private:
    void CollectDumpHeaderInfo(int pid);
    bool ParseMinidump();
    bool ParseExceptionStream(MinidumpParser& minidumpParser);
    std::shared_ptr<DfxRegs> CreateARM64DfxRegs(std::shared_ptr<MDRawContextARM64> arm64Context);
    bool BuildSignalInfoFromException(MinidumpException* exception);
    bool ParseThreadListStream(MinidumpParser& minidumpParser);
    bool SetupKeyThreadStack(MinidumpThreadList* threadList, std::shared_ptr<DfxThread> keyThread);
    void PopulateOtherThreadFromMinidump(std::shared_ptr<DfxThread> dfxThread, MinidumpThreadList* threadList);
    bool ParseThreadNameStream(MinidumpParser& minidumpParser);
    bool ParseModuleListStream(MinidumpParser& minidumpParser);
    bool ParseMemoryListStream(MinidumpParser& minidumpParser);
    bool ParseMapListStream(MinidumpParser& minidumpParser);

    void ConfigurePerformance(MinidumpParser& minidumpParser);
    bool TransferData(int srcFd, int dstFd);
    bool GenerateMinidump(int pid, pid_t pipeFd, bool enableMinidump);
    void UnwindProcess();
    void UnwindOtherThread();
    void PrintDumpInfo();
    void PrintHeader();
    void PrintThreadInfo();
    void PrintRegisters();
    void PrintOtherThreadInfo();
    void PrintRegsNearMemory();
    void PrintFaultStack();
    void PrintMaps();
    void Report();
    DfxProcess process_;
    ProcessDumpRequest request_ {};
    SmartFd outputFdGuard_;
    std::shared_ptr<DfxMaps> dfxMaps_;
    std::shared_ptr<Unwinder> unwinder_;
    FaultStack faultStack_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
#endif
