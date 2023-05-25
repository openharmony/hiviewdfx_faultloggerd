/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef DWARF_UNWINDER_H
#define DWARF_UNWINDER_H

#include <vector>
#include <libunwind.h>
#include "dfx_frame.h"
#include "dfx_symbols_cache.h"

namespace OHOS {
namespace HiviewDFX {
class DwarfUnwinder {
public:
    DwarfUnwinder();
    ~DwarfUnwinder();

    bool UnwindWithContext(unw_addr_space_t as, unw_context_t& context, std::shared_ptr<DfxSymbolsCache> cache,
        size_t skipFrameNum);
    bool Unwind(size_t skipFrameNum);
    const std::vector<DfxFrame>& GetFrames() const;
private:
    void UpdateFrameFuncName(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache, DfxFrame& frame);

private:
    std::vector<DfxFrame> frames_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
