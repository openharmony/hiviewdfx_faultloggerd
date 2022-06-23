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
#include <mutex>
#include <memory>
#include <string>
#include <thread>

#include "nocopyable.h"
#include "dfx_dump_request.h"
#include "dfx_ring_buffer.h"
#include "dfx_process.h"
#include "cppcrash_reporter.h"

namespace OHOS {
namespace HiviewDFX {
class ProcessDumper final {
public:
    static ProcessDumper &GetInstance();
    ~ProcessDumper() = default;

    void Dump(bool isSignalHdlr, ProcessDumpType type, int32_t pid, int32_t tid);
    
    void PrintDumpProcessMsg(std::string msg);
    int PrintDumpProcessBuf(const char *format, ...);

    DfxRingBuffer<BACK_TRACE_RING_BUFFER_SIZE, std::string> backTraceRingBuffer_;
private:
    static void LoopPrintBackTraceInfo();

    void DumpProcessWithSignalContext(std::shared_ptr<DfxProcess> &process, \
                                      std::shared_ptr<ProcessDumpRequest> request);
    void DumpProcessWithPidTid(std::shared_ptr<DfxProcess> &process, \
                               std::shared_ptr<ProcessDumpRequest> request);
    
    void InitPrintThread(int32_t fromSignalHandler, std::shared_ptr<ProcessDumpRequest> request, \
        std::shared_ptr<DfxProcess> process);
    void PrintDumpProcessWithSignalContextHeader(std::shared_ptr<DfxProcess> process, siginfo_t info,
                                                 const std::string& msg);
    void PrintDumpProcessFooter(std::shared_ptr<DfxProcess> process, bool printMapFlag);

    ProcessDumper() = default;
    DISALLOW_COPY_AND_MOVE(ProcessDumper);

    int32_t backTraceFileFd_;
    std::thread backTracePrintThread_;
    volatile bool backTraceIsFinished_ = false;
    std::shared_ptr<CppCrashReporter> reporter_ = nullptr;
    static std::condition_variable backTracePrintCV;
    static std::mutex backTracePrintMutx;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
