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
#ifndef DFX_FRAME_FORMAT_H
#define DFX_FRAME_FORMAT_H

#include <memory>
#include <string>
#include <vector>
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {
class DfxFrameFormat {
public:
    DfxFrameFormat() = default;
    ~DfxFrameFormat() = default;

    static std::string GetFrameStr(const DfxFrame& frame);
    static std::string GetFrameStr(const std::shared_ptr<DfxFrame>& frame);
    static std::string GetFramesStr(const std::vector<DfxFrame>& frames);
    static std::string GetFramesStr(const std::vector<std::shared_ptr<DfxFrame>>& frames);

    static std::vector<std::shared_ptr<DfxFrame>> ConvertFrames(const std::vector<DfxFrame>& frames);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
