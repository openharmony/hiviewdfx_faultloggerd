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
#ifndef ARM_EXIDX_H
#define ARM_EXIDX_H

#include <deque>
#include <memory>
#include <vector>
#include "dfx_errors.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMemory;

class ArmExidx {
public:
    ArmExidx(DfxMemory* memory) : memory_(memory) {}
    virtual ~ArmExidx() = default;

    bool ExtractEntryData(uint32_t entryOffset);
    bool Decode();
    bool Eval();
    int SearchUnwindTable(uintptr_t pc, UnwindDynInfo *di, UnwindProcInfo *pi, int needUnwindInfo);

    std::deque<uint8_t> GetData() { return data_; }
    void SetData(std::deque<uint8_t> &data) { data_ = data ; }
    UnwindErrorData lastErrorData_;

private:
    bool ReadPrel31(uintptr_t* addr, uintptr_t* val);
    void LogRawData();

protected:
    DfxMemory* memory_;
    std::deque<uint8_t> data_;
    uint32_t cfa_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
