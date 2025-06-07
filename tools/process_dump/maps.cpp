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
#include <set>
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_log.h"
#include "process_dump_config.h"
#include "unwinder.h"
namespace OHOS {
namespace HiviewDFX {
REGISTER_DUMP_INFO_CLASS(Maps);

void Maps::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    auto maps = unwinder.GetMaps();
    if (maps == nullptr) {
        DFXLOGE("maps is not init!");
        return;
    }
    DfxBufferWriter::GetInstance().WriteMsg("\nMaps:\n");
    if (ProcessDumpConfig::GetInstance().GetConfig().simplifyVmaPrinting ||
        (process.GetCrashLogConfig().simplifyVmaPrinting)) {
        std::set<DfxMap> simplifyMaps;
        SimplifyVma(process, request, maps, simplifyMaps);
        for (const auto& simplifyMap : simplifyMaps) {
            DfxBufferWriter::GetInstance().WriteMsg(simplifyMap.ToString());
        }
    } else {
        for (const auto& map : maps->GetMaps()) {
            if (map != nullptr) {
                DfxBufferWriter::GetInstance().WriteMsg(map->ToString());
            }
        }
    }
}

void Maps::SimplifyVma(DfxProcess& process, const ProcessDumpRequest& request,
                       const std::shared_ptr<DfxMaps>& maps, std::set<DfxMap>& mapSet)
{
    std::set<uintptr_t> interestedAddrs = process.GetMemoryValues();
    auto regsData = process.GetFaultThreadRegisters()->GetRegsData();
    interestedAddrs.insert(regsData.begin(), regsData.end());
    DfxSignal dfxSignal(request.siginfo.si_signo);
    if (dfxSignal.IsAddrAvailable()) {
        interestedAddrs.insert(reinterpret_cast<uintptr_t>(request.siginfo.si_addr));
    }
    auto threads = process.GetOtherThreads();
    threads.emplace_back(process.GetKeyThread());
    std::set<std::string> mapNames;
    for (const auto& thread : threads) {
        for (const auto &frame : thread->GetFrames()) {
            interestedAddrs.emplace(frame.pc);
        }
    }
    for (const auto& addr : interestedAddrs) {
        maps->FindMapGroupByAddr(addr, mapSet);
    }
}
}
}