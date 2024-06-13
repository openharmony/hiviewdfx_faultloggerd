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
#ifndef DFX_DUMP_RES_H
#define DFX_DUMP_RES_H

#include <cinttypes>
#include <string>
#include <sstream>

namespace OHOS {
namespace HiviewDFX {

/**
 * @brief Processdump error code
 * It describes the status of dumping.
 */
enum DumpErrorCode : int32_t {
    /** no error */
    DUMP_ESUCCESS = 0,
    /** read request error */
    DUMP_EREADREQUEST,
    /** ppid is crash */
    DUMP_EGETPPID,
    /** ptrace attach thread failed */
    DUMP_EATTACH,
    /** get fd error */
    DUMP_EGETFD,
    /** out of memory */
    DUMP_ENOMEM,
    /** bad register number */
    DUMP_EBADREG,
    /** attempt to write read-only register */
    DUMP_EREADONLYREG,
    /** stop unwinding */
    DUMP_ESTOPUNWIND,
    /** invalid IP */
    DUMP_EINVALIDIP,
    /** bad frame */
    DUMP_EBADFRAME,
    /** unsupported operation or bad value */
    DUMP_EINVAL,
    /** unwind info has unsupported version */
    DUMP_EBADVERSION,
    /** no unwind info found */
    DUMP_ENOINFO,
    /** no map info found */
    DUMP_ENOMAP,
};

class DfxDumpRes {
public:
    inline static std::string ToString(const int32_t& res)
    {
        std::stringstream ss;
        ss << std::to_string(res) << " ( " << GetResStr(res) << " )";
        return ss.str();
    }

private:
    inline static const char* GetResStr(const int32_t& res)
    {
        const char *cp;
        switch (res) {
            case DUMP_ESUCCESS:     cp = "no error"; break;
            case DUMP_EREADREQUEST: cp = "read dump request error"; break;
            case DUMP_EGETPPID:     cp = "ppid is crashed before unwind"; break;
            case DUMP_EATTACH:      cp = "ptrace attach thread failed"; break;
            case DUMP_EGETFD:       cp = "get fd error"; break;
            case DUMP_ENOMEM:       cp = "out of memory"; break;
            case DUMP_EBADREG:      cp = "bad register number"; break;
            case DUMP_EREADONLYREG: cp = "attempt to write read-only register"; break;
            case DUMP_ESTOPUNWIND:  cp = "stop unwinding"; break;
            case DUMP_EINVALIDIP:   cp = "invalid IP"; break;
            case DUMP_EBADFRAME:    cp = "bad frame"; break;
            case DUMP_EINVAL:       cp = "unsupported operation or bad value"; break;
            case DUMP_EBADVERSION:  cp = "unwind info has unsupported version"; break;
            case DUMP_ENOINFO:      cp = "no unwind info found"; break;
            case DUMP_ENOMAP:       cp = "mapinfo is not exist"; break;
            default:                cp = "invalid error code"; break;
        }
        return cp;
    }
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
