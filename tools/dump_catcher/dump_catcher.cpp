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

void DumpCatcher::Dump(int32_t pid, int32_t tid, int timeout) const
{
    DfxDumpCatcher dfxDump;
    std::string msg = "";
    auto dumpResult = dfxDump.DumpCatchWithTimeout(pid, msg, timeout, tid);
    std::string toFind = "Result:";
    size_t startPos = msg.find(toFind);
    if (startPos != std::string::npos) {
        size_t endPos = msg.find("\n", startPos);
        if (endPos != std::string::npos) {
            msg.erase(startPos, endPos - startPos + 1);
        }
    }
    if (dumpResult.first == -1) {
        printf("Result:dump failed.\n");
    } else if (dumpResult.first == 0) {
        printf("Result:dump normal stack success.\n");
    } else if (dumpResult.first == 1) {
        printf("Result:dump kernel stack success.\n");
    }
    printf("%s", dumpResult.second.c_str());
    printf("%s\n", msg.c_str());
}
} // namespace HiviewDFX
} // namespace OHOS
