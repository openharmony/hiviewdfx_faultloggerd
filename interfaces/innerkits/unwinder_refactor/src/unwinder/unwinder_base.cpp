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
#include "unwinder_base.h"

#include "dfx_ark.h"
#include "dfx_jsvm.h"
#include "dfx_param.h"
#include "dfx_trace_dlsym.h"
#include "unwind_entry_parser_factory.h"

namespace OHOS {
namespace HiviewDFX {

UnwinderBase::~UnwinderBase()
{
    OnDestroy();
    DfxEnableTraceDlsym(false);
    frameMgr_.Clear();
    unwindCore_.ClearCache();
#if defined(ENABLE_MIXSTACK)
    if (state_->isArkCreateLocal) {
        (void)DfxArk::Instance().ArkDestroyLocal();
    }
#endif
    if (state_->arkwebJsExtractorptr != 0) {
        DfxJsvm::Instance().JsvmDestroyJsSymbolExtractor(state_->arkwebJsExtractorptr);
    }
    DfxJsvm::Instance().Clear();
}

void UnwinderBase::InitCommon(std::shared_ptr<UnwindAccessors> accessors)
{
    if (accessors != nullptr) {
        state_->memory = std::make_shared<DfxMemory>(state_->unwindType, accessors);
    } else {
        state_->memory = std::make_shared<DfxMemory>(state_->unwindType);
    }
    enableFpCheckMapExec_ = true;
    state_->unwindEntryParser = UnwindEntryParserFactory::CreateUnwindEntryParser(state_->memory);
#if defined(ENABLE_MIXSTACK)
    state_->enableMixstack = DfxParam::IsEnableMixstack();
#endif
#if defined(__aarch64__)
    state_->pacMask = pacMaskDefault_;
#endif
}

} // namespace HiviewDFX
} // namespace OHOS
