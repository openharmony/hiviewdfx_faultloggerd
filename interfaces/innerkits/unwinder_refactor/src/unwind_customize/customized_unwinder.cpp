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
#include "customized_unwinder.h"

#include "dfx_trace_dlsym.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {

CustomizedUnwinder::CustomizedUnwinder(std::shared_ptr<UnwindAccessors> accessors, bool local)
    : UnwinderBase(std::make_shared<UnwinderSharedState>())
{
    state_->unwindType = local ? UNWIND_TYPE_CUSTOMIZE_LOCAL : UNWIND_TYPE_CUSTOMIZE;
    state_->pid = state_->unwindType;
    DfxEnableTraceDlsym(true);
    InitCommon(accessors);
    enableFpCheckMapExec_ = false;
    unwindCore_.SetEnableFpCheckMapExec(false);
    frameMgr_.SetEnableFillFrames(false);
}

} // namespace HiviewDFX
} // namespace OHOS
