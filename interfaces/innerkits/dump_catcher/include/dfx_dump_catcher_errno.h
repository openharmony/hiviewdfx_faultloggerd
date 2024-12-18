/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef DFX_DUMPCATCH_ERRNO_H
#define DFX_DUMPCATCH_ERRNO_H

#include <cinttypes>
#include <string>
#include <map>

namespace OHOS {
namespace HiviewDFX {

/**
 * @brief Dumpcatch error code
 * It describes the error status of dumpcatch.
 */
enum DumpCatchErrorCode : int32_t {
    DUMPCATCH_ESUCCESS = 0,                           /* success */

    DUMPCATCH_EPARAM = 101,                           /* param error */
    DUMPCATCH_NO_PROCESS,                             /* process has exited */
    DUMPCATCH_IS_DUMPING,                             /* process is dumping */
    DUMPCATCH_EPERMISSION,                            /* check permission error */
    DUMPCATCH_HAS_CRASHED,                            /* process has been crashed */
    DUMPCATCH_ECONNECT,                               /* failed to connect to faultloggerd */
    DUMPCATCH_EWRITE,                                 /* failed to write data to faultloggerd */
    DUMPCATCH_EFD,                                    /* buf or res fd error */
    DUMPCATCH_EPOLL,                                  /* poll error */

    DUMPCATCH_DUMP_EPTRACE = 201,                     /* failed to ptrace thread */
    DUMPCATCH_DUMP_EUNWIND,                           /* failed to unwind */
    DUMPCATCH_DUMP_EMAP,                              /* failed to find map */

    DUMPCATCH_TIMEOUT_SIGNAL_BLOCK = 301,             /* signal has been block by target process */
    DUMPCATCH_TIMEOUT_KERNEL_FROZEN,                  /* target process has been frozen in kernel */
    DUMPCATCH_TIMEOUT_PROCESS_KILLED,                 /* target process has been killed during dumping */
    DUMPCATCH_TIMEOUT_DUMP_SLOW,                      /* failed to fully dump due to timeout */

    DUMPCATCH_UNKNOWN = 400,                          /* unknown reason */

    DUMPCATCH_KERNELSTACK_ECREATE = 401,              /* kernelstack fail due to create hstackval fail */
    DUMPCATCH_KERNELSTACK_EOPEN,                      /* kernelstack fail due to open bbox fail */
    DUMPCATCH_KERNELSTACK_EIOCTL,                     /* kernelstack fail due to ioctl fail */
    DUMPCATCH_KERNELSTACK_NONEED,                     /* no need to dump kernelstack */
};

class DfxDumpCatchError {
public:
    static std::string ToString(const int32_t res);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif