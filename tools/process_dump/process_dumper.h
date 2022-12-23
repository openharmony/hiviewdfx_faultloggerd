/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains process dump hrader. */

#ifndef DFX_PROCESSDUMP_H
#define DFX_PROCESSDUMP_H

#include <cinttypes>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "cppcrash_reporter.h"
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "nocopyable.h"

namespace OHOS {
namespace HiviewDFX {
class ProcessDumper final {
public:
    static ProcessDumper &GetInstance();
    ~ProcessDumper() = default;

    void Dump();
    void WriteDumpRes(int32_t res);
    int32_t GetTargetPid();
    int32_t GetTargetNsPid();
private:
    static int WriteDumpBuf(int fd, const char* buf, const int len);
    int DumpProcessWithSignalContext(std::shared_ptr<ProcessDumpRequest> request);
    int InitPrintThread(bool fromSignalHandler, std::shared_ptr<ProcessDumpRequest> request);
    void PrintDumpProcessWithSignalContextHeader(std::shared_ptr<ProcessDumpRequest> request);
    void CreateVmProcessIfNeed(std::shared_ptr<ProcessDumpRequest> request, bool enableNs);
    bool InitProcessNsInfo(std::shared_ptr<ProcessDumpRequest> request, bool isCrash);
    int InitProcessInfo(std::shared_ptr<ProcessDumpRequest> request, bool isCrash, bool enableNs);

    ProcessDumper() = default;
    DISALLOW_COPY_AND_MOVE(ProcessDumper);

    std::shared_ptr<DfxProcess> vmProcess_ = nullptr;
    std::shared_ptr<DfxProcess> targetProcess_ = nullptr;
    std::shared_ptr<CppCrashReporter> reporter_ = nullptr;
    int32_t resFd_ = -1;
    int32_t resDump_ = 0;
    int32_t targetPid_ = -1;
    int32_t targetNsPid_ = -1;
    int32_t targetVmPid_ = -1;
    int32_t targetVmNsPid_ = -1;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
