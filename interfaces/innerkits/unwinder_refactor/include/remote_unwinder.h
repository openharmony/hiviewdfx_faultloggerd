/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef REMOTE_UNWINDER_H
#define REMOTE_UNWINDER_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "dfx_maps.h"
#include "unwinder_base.h"

namespace OHOS {
namespace HiviewDFX {

class RemoteUnwinder : public UnwinderBase {
public:
    RemoteUnwinder(int pid, bool crash, std::shared_ptr<DfxMaps> maps);
    RemoteUnwinder(int pid, int nspid, bool crash, std::shared_ptr<DfxMaps> maps);

    bool DoUnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum) override;

private:
    bool IsRemoteParamsValid(pid_t tid);
    bool PrepareRemoteRegs(pid_t tid, bool withRegs);
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // REMOTE_UNWINDER_H
