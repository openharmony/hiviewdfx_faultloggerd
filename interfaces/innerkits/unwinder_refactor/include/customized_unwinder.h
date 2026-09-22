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
#ifndef CUSTOMIZED_UNWINDER_H
#define CUSTOMIZED_UNWINDER_H

#include <memory>

#include "unwinder_base.h"

namespace OHOS {
namespace HiviewDFX {

class CustomizedUnwinder : public UnwinderBase {
public:
    CustomizedUnwinder(std::shared_ptr<UnwindAccessors> accessors, bool local);
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // CUSTOMIZED_UNWINDER_H
