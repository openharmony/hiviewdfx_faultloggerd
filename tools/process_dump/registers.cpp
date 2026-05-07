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
#include "dfx_log.h"
#include "dfx_buffer_writer.h"
#include "cppcrash_info_collector.h"
namespace OHOS {
namespace HiviewDFX {
REGISTER_DUMP_INFO_CLASS(Registers);

void Registers::Collect(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    // Registers of unwThread has been changed, we should print regs from request context.
    process.SetFaultThreadRegisters(DfxRegs::CreateFromUcontext(request.context));
    if (process.GetFaultThreadRegisters() == nullptr) {
        DFXLOGE("Fault thread regs is nullptr!");
        return;
    }

    regsStr_ = process.GetFaultThreadRegisters()->PrintRegs();
    std::string prefix = "Registers:\n";
    std::string regsSubStr;
    if (regsStr_.size() >= prefix.size() && regsStr_.substr(0, prefix.size()) == prefix) {
        regsSubStr = regsStr_.substr(prefix.size());
    } else {
        regsSubStr = regsStr_;
    }
    CppCrashInfoCollector::Instance().SetRegisters(regsSubStr);
}

void Registers::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    DfxBufferWriter::GetInstance().WriteMsg(regsStr_);
    DfxBufferWriter::GetInstance().AppendBriefDumpInfo(regsStr_);
}
}
}