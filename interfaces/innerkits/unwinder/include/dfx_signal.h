/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_SIGNAL_H
#define DFX_SIGNAL_H

#include <csignal>
#include <cstdint>
#include <string>

namespace OHOS {
namespace HiviewDFX {
class DfxSignal {
public:
    explicit DfxSignal(const int32_t signal) : signal_(signal) {}
    ~DfxSignal() = default;

    static std::string PrintSignal(const siginfo_t &info);

    bool IsAvailable() const;
    bool IsAddrAvailable() const;
    bool IsPidAvailable() const;
    int32_t GetSignal() const
    {
        return signal_;
    }

private:
    DfxSignal() = delete;

    static std::string FormatSignalName(const int32_t signal);
    static std::string FormatCodeName(const int32_t signal, const int32_t signalCode);
    static std::string FormatSIGBUSCodeName(const int32_t signalCode);
    static std::string FormatSIGILLCodeName(const int32_t signalCode);
    static std::string FormatSIGFPECodeName(const int32_t signalCode);
    static std::string FormatSIGSEGVCodeName(const int32_t signalCode);
    static std::string FormatSIGTRAPCodeName(const int32_t signalCode);
    static std::string FormatCommonSignalCodeName(const int32_t signalCode);
private:
    int32_t signal_ = 0;
};
} // namespace Dfx
} // namespace OHOS
#endif
