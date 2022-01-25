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

/* This files contains process dump hrader. */

#ifndef DFX_PROCESSDUMP_H
#define DFX_PROCESSDUMP_H

#include <cinttypes>
#include <memory>

#include "nocopyable.h"
#include "dfx_dump_writer.h"

namespace OHOS {
namespace HiviewDFX {
class ProcessDumper final {
public:
    static ProcessDumper &GetInstance();

    void Dump(bool isSignalHdlr, ProcessDumpType type, int32_t pid, int32_t tid);
    ~ProcessDumper() = default;
    void SetDisplayBacktrace(bool displayBacktrace);
    bool GetDisplayBacktrace() const;
    void SetDisplayRegister(bool displayRegister);
    bool GetDisplayRegister() const;
    void SetDisplayMaps(bool Maps);
    bool GetDisplayMaps() const;
    void SetLogPersist(bool logPersist);
    bool GetLogPersist() const;
private:
    void DumpProcessWithSignalContext(std::shared_ptr<DfxProcess> &process,
                                      std::shared_ptr<ProcessDumpRequest> request);
    void DumpProcess(std::shared_ptr<DfxProcess> &process, std::shared_ptr<ProcessDumpRequest> request);

    ProcessDumper() = default;
    DISALLOW_COPY_AND_MOVE(ProcessDumper);
    bool displayBacktrace_ = true;
    bool displayRegister_ = true;
    bool displayMaps_ = true;
    bool logPersist_ = false;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_PROCESSDUMP_H
