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
#ifndef DFX_CONFIG_H
#define DFX_CONFIG_H

#include "iosfwd"

namespace OHOS {
namespace HiviewDFX {
class DfxConfig final {
public:
    static DfxConfig &GetInstance();

    void ReadConfig();
    void ParserConfig(const std::string& key, const std::string& value);
    void SetDisplayBacktrace(bool displayBacktrace);
    bool GetDisplayBacktrace() const;
    void SetDisplayRegister(bool displayRegister);
    bool GetDisplayRegister() const;
    void SetDisplayMaps(bool displayMaps);
    bool GetDisplayMaps() const;
    void SetDisplayFaultStack(bool displayFaultStack);
    bool GetDisplayFaultStack() const;
    void SetFaultStackLowAddressStep(unsigned int lowAddressStep);
    unsigned int GetFaultStackLowAddressStep() const;
    void SetFaultStackHighAddressStep(unsigned int highAddressStep);
    unsigned int GetFaultStackHighAddressStep() const;
    void SetLogPersist(bool logPersist);
    bool GetLogPersist() const;
    void SetDumpOtherThreads(bool dumpOtherThreads);
    bool GetDumpOtherThreads() const;

private:
    DfxConfig() = default;
    ~DfxConfig() = default;
    bool displayBacktrace_ = true;
    bool displayRegister_ = true;
    bool displayMaps_ = true;
    bool displayFaultStack_ = true;
    bool logPersist_ = false;
    bool dumpOtherThreads_ = false;
    unsigned int lowAddressStep_ = 16;
    unsigned int highAddressStep_ = 64;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif
