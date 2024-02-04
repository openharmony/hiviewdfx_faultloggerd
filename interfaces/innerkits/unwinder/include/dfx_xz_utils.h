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

#ifndef DFX_XZ_UTILS_H
#define DFX_XZ_UTILS_H
#include <malloc.h>
#include <memory>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
__attribute__ ((visibility("hidden"))) bool XzDecompress(const uint8_t *src, size_t srcLen,
    std::shared_ptr<std::vector<uint8_t>> out);
}   // namespace HiviewDFX
}   // namespace OHOS
#endif