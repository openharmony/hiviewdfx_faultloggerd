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

/* This files contains process dump hrader. */

#ifndef DFX_DUMP_CATCHER_H
#define DFX_DUMP_CATCHER_H

#include <cinttypes>
#include "nocopyable.h"

namespace OHOS {
namespace HiviewDFX {
class DumpCatcher final {
public:
    static DumpCatcher &GetInstance();
    ~DumpCatcher() = default;

    void Dump(int32_t pid, int32_t tid) const;

private:
    DumpCatcher() = default;
    DISALLOW_COPY_AND_MOVE(DumpCatcher);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
