/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "arm_exidx.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "dfx_memory.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxArmExidx"

#define ARM_EXIDX_CANT_UNWIND   0x00000001
#define ARM_EXIDX_COMPACT       0x80000000
#define ARM_EXTBL_OP_FINISH     0xb0
}

void ArmExidx::LogRawData()
{
    std::string logStr("Raw Data:");
    for (const uint8_t data : data_) {
        logStr += StringPrintf(" 0x%02x", data);
    }
    LOGU("%s", logStr.c_str());
}

bool ArmExidx::ExtractEntryData(uint32_t entryOffset)
{
    LogRawData();
    return true;
}

bool ArmExidx::Eval()
{
    while (Decode());
    return lastErrorData_.code == UNW_ERROR_NONE;
}

bool ArmExidx::Decode()
{
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS