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

/* This files contains process dump write log to file module. */

#ifndef DFX_DUMP_WIRTTER_H
#define DFX_DUMP_WIRTTER_H

#include <stdint.h>       // for int32_t
#include <memory>         // for shared_ptr
#include "dfx_process.h"  // for DfxProcess

namespace OHOS {
namespace HiviewDFX {
class DfxDumpWriter {
public:
    DfxDumpWriter() = default;
    virtual ~DfxDumpWriter() = default;
    DfxDumpWriter(std::shared_ptr<DfxProcess> process, int32_t fromSignalHandler);
private:
    std::shared_ptr<DfxProcess> process_ = nullptr;
    int32_t fromSignalHandler_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
