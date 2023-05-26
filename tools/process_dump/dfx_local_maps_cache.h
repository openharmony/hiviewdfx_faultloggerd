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

/* This files contains feader file of maps module. */

#ifndef DFX_LOCAL_MAPS_CACHE_H
#define DFX_LOCAL_MAPS_CACHE_H

#include <string>
#include <mutex>
#include "dfx_maps.h"

namespace OHOS {
namespace HiviewDFX {
class DfxLocalMapsCache {
public:
    DfxLocalMapsCache() = default;
    ~DfxLocalMapsCache() = default;
    static void UpdateLocalMaps();
    static void ReleaseLocalMaps();
    static std::shared_ptr<DfxElfMaps> &GetMapsCache();
    static bool FindMapByAddrAsUpdate(uintptr_t address, std::shared_ptr<DfxElfMap>& map);
private:
    static std::shared_ptr<DfxElfMaps> currentMaps_;
    static std::mutex mutex_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
