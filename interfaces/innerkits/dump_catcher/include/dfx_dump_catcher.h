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

/* This files contains sdk dump catcher module. */

#ifndef DFX_DUMPCATCH_H
#define DFX_DUMPCATCH_H

#include <cinttypes>
#include <cstring>
#include <string>

#include "dfx_dump_catcher_local_dumper.h"

namespace OHOS {
namespace HiviewDFX {
class DfxDumpCatcher {
public:
    DfxDumpCatcher();
    ~DfxDumpCatcher();
    bool DumpCatch(int pid, int tid, std::string& msg);
    bool DumpCatchMultiPid(const std::vector<int> pidV, std::string& msg);
    bool DumpCatchFrame(int pid, int tid, std::string& msg, \
        std::vector<std::shared_ptr<DfxDumpCatcherFrame>>& frameV);

private:
    bool DoDumpLocalTid(int tid);
    bool DoDumpLocalPid(int pid, std::string& msg);
    bool DoDumpLocalLocked(int pid, int tid, std::string& msg);
    bool DoDumpRemoteLocked(int pid, int tid, std::string& msg);
    std::string WaitForLogGenerate(const std::string& path, const std::string& prefix);
    std::string TryToGetGeneratedLog(const std::string& path, const std::string& prefix);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
