/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains sdk dump catcher module. */

#ifndef DFX_DUMPCATCH_H
#define DFX_DUMPCATCH_H

#include <cinttypes>
#include <cstring>
#include <string>
#include <unistd.h>
#include <ucontext.h>

#include "faultloggerd_client.h"
#include "dfx_frames.h"

namespace OHOS {
namespace HiviewDFX {
class DfxDumpCatcher {
public:
    DfxDumpCatcher();
    ~DfxDumpCatcher();
    bool DumpCatch(const int pid, const int tid, std::string& msg);

private:
    bool ExecLocalDump(const int pid, const int tid, std::string& msg);
    long WriteDumpInfo(long current_position, size_t index, std::shared_ptr<DfxFrames> frame);
    void FreeStackInfo();

private:
    char* g_StackInfo_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
