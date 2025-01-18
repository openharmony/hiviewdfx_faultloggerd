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

#ifndef ELF_FACTORY_SELECTOR_H
#define ELF_FACTORY_SELECTOR_H

#include <memory>
#include "elf_factory.h"
#include "dfx_map.h"

namespace OHOS {
namespace HiviewDFX {
class ElfFactorySelector {
public:
    static std::shared_ptr<ElfFactory> Select(DfxMap* map, pid_t pid = 0);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
