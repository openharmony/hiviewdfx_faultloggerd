
/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "dfx_log.h"
#include "elf_factory.h"
#include "elf_factory_selector.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "ElfFactorySelector"
}
std::shared_ptr<ElfFactory> ElfFactorySelector::Select(DfxMap* map, pid_t pid)
{
    if (map == nullptr) {
        DFXLOGE("map is null!");
        return nullptr;
    }
    std::shared_ptr<ElfFactory> elfFactory = nullptr;
    if (EndsWith(map->name, ".hap")) {
        elfFactory = std::make_shared<CompressHapElfFactory>(map->name, map->prevMap, map->offset);
    } else if (map->IsVdsoMap()) {
        elfFactory = std::make_shared<VdsoElfFactory>(map->begin, map->end - map->begin, pid);
    } else {
        elfFactory = std::make_shared<RegularElfFactory>(map->name);
    }
    return elfFactory;
}
} // namespace HiviewDFX
} // namespace OHOS