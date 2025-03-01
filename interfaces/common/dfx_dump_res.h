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
    /** fail to read real pid */
    DUMP_EREADPID,
};

class DfxDumpRes {
public:
    inline static std::string ToString(int32_t res)
    {
        return std::to_string(res) + " ( " + GetResStr(res) + " )";
    }

private:
    struct DumpErrInfo {
        int32_t errCode;
        const char *info;
    };
    static const char* GetResStr(int32_t res)
    {
        const DumpErrInfo errInfos[] = {
            { DUMP_ESUCCESS, "no error" },
            { DUMP_EREADREQUEST, "read dump request error" },
            { DUMP_EATTACH, "ptrace attach thread failed" },
            { DUMP_EGETFD, "get fd error" },
            { DUMP_ENOMEM, "out of memory" },
            { DUMP_EBADREG, "bad register number" },
            { DUMP_EREADONLYREG, "attempt to write read-only register" },
            { DUMP_ESTOPUNWIND, "stop unwinding" },
            { DUMP_EINVALIDIP, "invalid IP" },
            { DUMP_EBADFRAME, "bad frame" },
            { DUMP_EINVAL, "unsupported operation or bad value" },
            { DUMP_EBADVERSION, "unwind info has unsupported version" },
            { DUMP_ENOINFO, "no unwind info found" },
            { DUMP_ENOMAP, "mapinfo is not exist" },
            { DUMP_EREADPID, "fail to read real pid" },
        };

        auto iter = std::find_if(std::begin(errInfos), std::end(errInfos), [res](const DumpErrInfo &ele) {
            return ele.errCode == res;
        });
        return iter != std::end(errInfos) ? iter->info : "invalid error code";
    }
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
