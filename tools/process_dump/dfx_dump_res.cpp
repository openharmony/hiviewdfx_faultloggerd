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
#include "dfx_dump_res.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
DfxDumpRes &DfxDumpRes::GetInstance()
{
    static DfxDumpRes ins;
    return ins;
}

void DfxDumpRes::SetRes(int32_t res)
{
    resMsg_.res = res;
}

int32_t DfxDumpRes::GetRes() const
{
    return resMsg_.res;
}

const char* DfxDumpRes::GetResStr(const int res)
{
    const char *cp;
    switch (res) {
        case DUMP_ESUCCESS:     cp = "no error"; break;
        case DUMP_EGETPPID:     cp = "ppid is crashed before unwind"; break;
        case DUMP_EUNSPEC:      cp = "unspecified (general) error"; break;
        case DUMP_ENOMEM:       cp = "out of memory"; break;
        case DUMP_EBADREG:      cp = "bad register number"; break;
        case DUMP_EREADONLYREG: cp = "attempt to write read-only register"; break;
        case DUMP_ESTOPUNWIND:  cp = "stop unwinding"; break;
        case DUMP_EINVALIDIP:   cp = "invalid IP"; break;
        case DUMP_EBADFRAME:    cp = "bad frame"; break;
        case DUMP_EINVAL:       cp = "unsupported operation or bad value"; break;
        case DUMP_EBADVERSION:  cp = "unwind info has unsupported version"; break;
        case DUMP_ENOINFO:      cp = "no unwind info found"; break;
        default:                cp = "invalid error code"; break;
    }
    return cp;
}

std::string DfxDumpRes::ToString()
{
    std::stringstream ss;
    ss << std::to_string(resMsg_.res) << " ( " << GetResStr(resMsg_.res) << " )\n";
    return ss.str();
}
} // namespace HiviewDFX
} // namespace OHOS