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
#include "lock_parser.h"

#include <vector>

#include <pthread.h>

#include "dfx_log.h"
namespace OHOS {
namespace HiviewDFX {
namespace {
// defined in alltypes.h
#define DFX_MUTEX_TYPE u.i[0]
#define DFX_MUTEX_OWNER u.vi[1]
typedef struct {
    union {
        int i[sizeof(long) == 8 ? 10 : 6];
        volatile int vi[sizeof(long) == 8 ? 10 : 6];
        volatile void *volatile p[sizeof(long) == 8 ? 5 : 6];
    } u;
} DfxMutex;
}
bool LockParser::ParseLockInfo(Unwinder& unwinder, int32_t vmPid, int32_t tid)
{
#ifdef __aarch64__
    std::vector<char> buffer(sizeof(pthread_mutex_t), 0);
    if (unwinder.GetLockInfo(vmPid, buffer.data(), sizeof(pthread_mutex_t))) {
        auto mutexInfo = reinterpret_cast<DfxMutex*>(buffer.data());
        // only PTHREAD_MUTEX_ERRORCHECK(2) type lock contains lock holder thread-id
        // the normal type only store EBUSY in owner section
        int type = mutexInfo->DFX_MUTEX_TYPE;
        uint32_t owner = static_cast<uint32_t>(mutexInfo->DFX_MUTEX_OWNER) & 0x3fffffff;
        DFXLOGI("Thread(%{public}d) is waiting a lock with type %{public}d held by %{public}u", tid, type, owner);
        return true;
    }
    return false;
#else
    // not impl yet
    return false;
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
