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

/* This files contains dumpcatcher main module. */

#include "dump_catcher.h"
#include <iostream>
#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_dump_catcher.h"

namespace OHOS {
namespace HiviewDFX {
DumpCatcher &DumpCatcher::GetInstance()
{
    static DumpCatcher ins;
    return ins;
}

void DumpCatcher::Dump(int32_t pid, int32_t tid)
{
    DfxDumpCatcher dumplog;
    std::string msg = "";
    if (dumplog.DumpCatch(pid, tid, msg)) {
        if (!msg.empty()) {
            std::cout << msg << std::endl;
        } else {
            std::cout << "Dump Failed." << std::endl;
        }
    } else {
        std::cout << "Dump Failed." << std::endl;
        DfxLogWarn("DumpCatch fail.");
    }
}
} // namespace HiviewDFX
} // namespace OHOS
