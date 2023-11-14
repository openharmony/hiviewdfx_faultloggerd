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
#ifndef DFX_INSTR_STATISTIC_H
#define DFX_INSTR_STATISTIC_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef INSTR_STATISTIC_ENABLE
#define INSTR_STATISTIC_ELF(elf) \
    OHOS::HiviewDFX::DfxInstrStatistic::GetInstance().SetCurrentStatLib(elf)
#define INSTR_STATISTIC(Type, Arg1, Arg2) \
    OHOS::HiviewDFX::DfxInstrStatistic::GetInstance().AddInstrStatistic(Type, (uint64_t)Arg1, (uint64_t)Arg2)
#else
#define INSTR_STATISTIC_ELF(elf)
#define INSTR_STATISTIC(Type, Arg1, Arg2)
#endif

namespace OHOS {
namespace HiviewDFX {
enum InstrStatisticType : uint32_t {
    InstructionEntriesArmExidx = 0,
    InstructionEntriesEhFrame,
    InstructionEntriesDebugFrame,

    UnsupportedArmExidx = 10,
    UnsupportedDwarfOp_Reg,
    UnsupportedDwarfOp_Regx,
    UnsupportedDwarfOp_Breg,
    UnsupportedDwarfOp_Bregx,
    UnsupportedDwCfaOffset,
    UnsupportedDwCfaValOffset,
    UnsupportedDwCfaRegister,
    UnsupportedDwCfaExpr,
    UnsupportedDwCfaValExpr,
    UnsupportedDwCfaRestore,
    UnsupportedDwCfaUndefined,
    UnsupportedDwCfaSame,
    UnsupportedDefCfa,
};

class DfxInstrStatistic {
public:
    typedef std::unordered_map<uint32_t, std::shared_ptr<std::vector<std::pair<uint64_t, uint64_t>>>> STAT_INFO_MAP;
    static DfxInstrStatistic &GetInstance();
    virtual ~DfxInstrStatistic() { statisticInfo_.clear(); }

    void SetCurrentStatLib(const std::string soName);
    void AddInstrStatistic(InstrStatisticType type, uint64_t val, uint64_t err);
    void DumpInstrStatResult(std::vector<std::pair<uint32_t, uint32_t>> &result);
private:
    DfxInstrStatistic() = default;
    std::string soName_;
    STAT_INFO_MAP statisticInfo_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif