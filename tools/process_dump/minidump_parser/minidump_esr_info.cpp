/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <cinttypes>
#include <securec.h>

#include "dfx_log.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpEsrInfo::MinidumpEsrInfo(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader),
      esrRegsInfo_() {
}

bool MinidumpEsrInfo::Read(uint32_t expectedSize)
{
    isValid_ = false;

    if (expectedSize < sizeof(esrRegsInfo_)) {
        DFXLOGE("MinidumpEsrInfo expected size too small: %{public}u", expectedSize);
        return false;
    }

    (void)memset_s(&esrRegsInfo_, sizeof(esrRegsInfo_), 0, sizeof(esrRegsInfo_));

    if (!memoryReader_->ReadBytes(&esrRegsInfo_, sizeof(esrRegsInfo_))) {
        DFXLOGE("MinidumpEsrInfo cannot read breakpad info");
        return false;
    }

    isValid_ = true;
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("EsrInfo", 1);
    }
    return true;
}

void MinidumpEsrInfo::Print()
{
    if (!isValid_) {
        DFXLOGE("MDRawEsrRegsInfo cannot print invalid data");
        return;
    }

    DFXLOGI("MDRawEsrRegsInfo");
    DFXLOGI("  validity     = 0x%{public}x", esrRegsInfo_.validity);

    if (esrRegsInfo_.validity & MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID) {
        DFXLOGI("  dumpThreadId = %{public}u", esrRegsInfo_.dumpThreadId);
    }

    if (esrRegsInfo_.validity & MD_BREAKPAD_INFO_VALID_ESR_REGS) {
        DFXLOGI("  esrRegs      = 0x%{public}" PRIx64, esrRegsInfo_.esrRegs);
    }

    DFXLOGI("\n");
}
}
}