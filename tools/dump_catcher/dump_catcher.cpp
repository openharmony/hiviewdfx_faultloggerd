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

#include "dump_catcher.h"

#include "dfx_define.h"
#include "dfx_dump_catcher.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
DumpCatcher &DumpCatcher::GetInstance()
{
    static DumpCatcher ins;
    return ins;
}

void DumpCatcher::Dump(int32_t pid, int32_t tid) const
{
    DfxDumpCatcher dfxDump;
    std::string msg = "";
    bool dumpRet = false;
    dumpRet = dfxDump.DumpCatch(pid, tid, msg);
    if (!dumpRet) {
        printf("Dump Failed.\n");
    }
    if (!msg.empty()) {
        printf("%s\n", msg.c_str());
    } else {
        printf("Dump msg empty.\n");
    }
}
} // namespace HiviewDFX
} // namespace OHOS
