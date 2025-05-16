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
#ifndef PROCESS_DUMP_CONFIG_H
#define PROCESS_DUMP_CONFIG_H

#include <map>
#include <vector>
#include <string>
#include "dfx_dump_request.h"

namespace OHOS {
namespace HiviewDFX {
struct DumpConfig {
    int32_t maxFrameNums = 256;
    int32_t faultStackLowAddrStep = 32;
    int32_t faultStackHighAddrStep = 512;
    bool extendPcLrPrinting = false;
    bool simplifyVmaPrinting = false;
    int32_t reservedParseSymbolTime = 100;
    uint32_t logFileCutoffSizeBytes = 0;
    std::map<ProcessDumpType, std::vector<std::string>> dumpInfo;
};
class ProcessDumpConfig {
public:
    ProcessDumpConfig(const ProcessDumpConfig&) = delete;
    ProcessDumpConfig(ProcessDumpConfig&&) = delete;

    ProcessDumpConfig &operator=(const ProcessDumpConfig&) = delete;
    ProcessDumpConfig &operator=(ProcessDumpConfig&&) = delete;
    static ProcessDumpConfig& GetInstance();
    [[nodiscard]] const DumpConfig& GetConfig() const;
    void SetConfig(DumpConfig&& dumpConfig)
    {
        dumpConfig_ = std::move(dumpConfig);
    }
private:
    ProcessDumpConfig();
    ~ProcessDumpConfig() = default;
    void LoadDefaultConfig();
    DumpConfig dumpConfig_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
