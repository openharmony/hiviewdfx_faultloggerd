/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
class ProcessDumper final {
public:
    static ProcessDumper &GetInstance();
    ~ProcessDumper() = default;

    void Dump();
    void WriteDumpRes(int32_t res);
    bool IsCrash() const;

private:
    ProcessDumper() = default;
    DISALLOW_COPY_AND_MOVE(ProcessDumper);
    static int WriteDumpBuf(int fd, const char* buf, const int len);
    int32_t CreateFileForCrash(int32_t pid, uint64_t time) const;
    int DumpProcess(std::shared_ptr<ProcessDumpRequest> request);
    bool InitKeyThread(std::shared_ptr<ProcessDumpRequest> request);
    int InitPrintThread(std::shared_ptr<ProcessDumpRequest> request);
    int InitProcessInfo(std::shared_ptr<ProcessDumpRequest> request);
    bool InitVmThread(std::shared_ptr<ProcessDumpRequest> request);
    bool InitUnwinder(std::shared_ptr<ProcessDumpRequest> request, pid_t &vmPid, int &dumpRes);
    void InitRegs(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes);
    bool IsTargetProcessAlive(std::shared_ptr<ProcessDumpRequest> request);
    bool Unwind(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes, pid_t vmPid);
    static int GetLogTypeBySignal(int sig);
    void ReportSigDumpStats(const std::shared_ptr<ProcessDumpRequest> &request) const;
    void ReportCrashInfo(const std::string& jsonInfo);
    void UnwindWriteJit(const ProcessDumpRequest &request);
    std::string ReadStringByPtrace(pid_t tid, uintptr_t addr);
    void GetCrashObj(std::shared_ptr<ProcessDumpRequest> request);

private:
    std::shared_ptr<DfxProcess> process_ = nullptr;
    std::shared_ptr<CppCrashReporter> reporter_ = nullptr;
    std::shared_ptr<Unwinder> unwinder_ = nullptr;

    bool isCrash_ = false;
    bool isJsonDump_ = false;
    int32_t resFd_ = -1;
    int32_t jsonFd_ = -1;
    int32_t resDump_ = 0;

    uint64_t startTime_ = 0;
    uint64_t finishTime_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
