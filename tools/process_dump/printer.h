/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains process dump write log to file module. */

#ifndef CPP_CRASH_PRINTER_H
#define CPP_CRASH_PRINTER_H

#include <cinttypes>
#include <csignal>
#include <memory>
#include <string>
#include "nocopyable.h"
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "dfx_thread.h"

namespace OHOS {
namespace HiviewDFX {
class Printer {
public:
    static Printer &GetInstance();
    virtual ~Printer() {};

    void PrintDumpHeader(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process);
    void PrintProcessMapsByConfig(std::shared_ptr<DfxProcess> process);
    void PrintThreadsHeaderByConfig();
    void PrintThreadBacktraceByConfig(std::shared_ptr<DfxThread> thread);
    void PrintThreadRegsByConfig(std::shared_ptr<DfxThread> thread);
    void PrintThreadFaultStackByConfig(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread);

private:
    Printer() = default;
    DISALLOW_COPY_AND_MOVE(Printer);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
