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
#ifndef DFX_FRAME_FORMATTER_H
#define DFX_FRAME_FORMATTER_H

#include <memory>
#include <string>
#include <vector>
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {
class DfxFrameFormatter {
public:
    DfxFrameFormatter() = default;
    ~DfxFrameFormatter() = default;

    /**
     * @brief Get the Frame Str object
     *
     * @param frame native frame object
     * @return std::string native frame string
     */
    static std::string GetFrameStr(const DfxFrame& frame);

    /**
     * @brief Get the Frame Str object
     *
     * @param frame native frame pointer object
     * @return std::string native frame string
     */
    static std::string GetFrameStr(const std::shared_ptr<DfxFrame>& frame);

    /**
     * @brief Get the Frames Str object
     *
     * @param frames native frame object list
     * @return std::string native frames string
     */
    static std::string GetFramesStr(const std::vector<DfxFrame>& frames);

    /**
     * @brief Get the Frames Str object
     *
     * @param frames native frame pointer list
     * @return std::string native frames string
     */
    static std::string GetFramesStr(const std::vector<std::shared_ptr<DfxFrame>>& frames);

    /**
     * @brief Convert native frame object lists to native frame pointer list
     *
     * @param frames native frame object list
     * @return std::vector<std::shared_ptr<DfxFrame>> native frame pointer list
     */
    static std::vector<std::shared_ptr<DfxFrame>> ConvertFrames(const std::vector<DfxFrame>& frames);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
