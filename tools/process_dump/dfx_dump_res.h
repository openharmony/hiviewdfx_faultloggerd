/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "iosfwd"    // for string

namespace OHOS {
namespace HiviewDFX {
enum ProcessDumpRes {
    DUMP_ESUCCESS = 0,  /* no error */
    DUMP_EREADREQUEST,  /* read request error */
    DUMP_EGETPPID,      /* ppid is crash */
    DUMP_EATTACH,       /* ptrace attach thread failed */
    DUMP_EGETFD,        /* get fd error */
    DUMP_ENOMEM,        /* out of memory */
    DUMP_EBADREG,       /* bad register number */
    DUMP_EREADONLYREG,  /* attempt to write read-only register */
    DUMP_ESTOPUNWIND,   /* stop unwinding */
    DUMP_EINVALIDIP,    /* invalid IP */
    DUMP_EBADFRAME,     /* bad frame */
    DUMP_EINVAL,        /* unsupported operation or bad value */
    DUMP_EBADVERSION,   /* unwind info has unsupported version */
    DUMP_ENOINFO,       /* no unwind info found */
};

struct DumpResMsg {
    int32_t res;
    char strRes[128];
} __attribute__((packed));

class DfxDumpRes final {
public:
    static DfxDumpRes &GetInstance();

    int32_t GetRes() const;
    void SetRes(int32_t res);

    const char *GetResStr(const int res) const;

    std::string ToString() const;
private:
    DfxDumpRes() = default;
    ~DfxDumpRes() = default;

    DumpResMsg resMsg_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
