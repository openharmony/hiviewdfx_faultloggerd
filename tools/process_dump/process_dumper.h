/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#include "reporter.h"
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "dump_info.h"
#include "nocopyable.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
enum DumpType  {
    THREAD_STATUS_INVALID = -1,
    THREAD_STATUS_INIT = 0,
    THREAD_STATUS_ATTACHED = 1
};
class ProcessDumper final {
public:
    static ProcessDumper &GetInstance();
    void Dump();

private:
    ProcessDumper() = default;
    DISALLOW_COPY_AND_MOVE(ProcessDumper);
    int DumpProcess(ProcessDumpRequest& request);
    int32_t ReadRequestAndCheck(ProcessDumpRequest& request);
    bool InitBufferWriter(const ProcessDumpRequest& request);
    bool InitDfxProcess(ProcessDumpRequest& request);
    bool InitUnwinder(const ProcessDumpRequest& request, int &dumpRes);
    int GeFaultloggerdRequestType(const ProcessDumpRequest &request);
    void ReportSigDumpStats(const ProcessDumpRequest& request, uint64_t startTime, uint64_t finishTime) const;
    void UnwindWriteJit(const ProcessDumpRequest &request);
    void FormatJsonInfoIfNeed(const ProcessDumpRequest& request);
    void UpdateConfigByRequest(const ProcessDumpRequest &request);
    void WriteDumpResIfNeed(const ProcessDumpRequest& request, int32_t resDump);
    void PrintDumpInfo(const ProcessDumpRequest& request, int& dumpRes);
    int ParseSymbols(const ProcessDumpRequest& request, std::shared_ptr<DumpInfo> threadDumpInfo);
    std::vector<std::string> FindDumpInfoByType(const ProcessDumpType& dumpType);
    int32_t CreateFileForCrash(int32_t pid, uint64_t time) const;
    void RemoveFileIfNeed(const std::string& dirPath) const;

private:
    void SetProcessdumpTimeout(siginfo_t &si);
    std::shared_ptr<DfxProcess> process_ = nullptr;
    std::shared_ptr<Unwinder> unwinder_ = nullptr;

    static constexpr size_t DEFAULT_MAX_STRING_LEN = 2048;
    bool isJsonDump_ = false;
    uint64_t expectedDumpFinishTime_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
