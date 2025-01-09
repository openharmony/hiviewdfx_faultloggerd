/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef CPP_CRASH_PRINTER_H
#define CPP_CRASH_PRINTER_H

#include <cinttypes>
#include <csignal>
#include <memory>
#include <string>

#include "dfx_dump_request.h"
#include "dfx_frame.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
class Printer {
public:
    static void PrintDumpHeader(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                                std::shared_ptr<Unwinder> unwinder);
    static void PrintProcessMapsByConfig(std::shared_ptr<DfxMaps> maps);
    static void PrintOtherThreadHeaderByConfig();
    static void PrintThreadHeaderByConfig(std::shared_ptr<DfxThread> thread, bool isKeyThread);
    static void PrintThreadBacktraceByConfig(std::shared_ptr<DfxThread> thread, bool isKeyThread);
    static void PrintThreadRegsByConfig(std::shared_ptr<DfxThread> thread);
    static void PrintRegsByConfig(std::shared_ptr<DfxRegs> regs);
    static void CollectThreadFaultStackByConfig(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread,
                                              std::shared_ptr<Unwinder> unwinder);
    static void PrintThreadFaultStackByConfig(std::shared_ptr<DfxThread> thread);
    static void PrintLongInformation(const std::string& info);
    static bool IsLastValidFrame(const DfxFrame& frame);
private:
    static void PrintReason(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process,
                            std::shared_ptr<Unwinder> unwinder, std::string& reasonInfo);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
