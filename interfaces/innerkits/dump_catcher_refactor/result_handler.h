/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef RESULT_HANDLER_H
#define RESULT_HANDLER_H

#include <cstdint>
#include <string>

#include "dump_catcher_pipe_data.h"
#include "dump_catcher_shared_state.h"
#include "timeout_analyzer.h"

namespace OHOS {
namespace HiviewDFX {

class ResultHandler {
public:
    explicit ResultHandler(DumpCatcherSharedState& state) : state_(state) {}

    void DealWithPollRet(int pollRet, int pid, int32_t& ret, std::string& msg, DumpCatcherPipeData& pipeData);
    void DealWithSdkDumpRet(int sdkdumpRet, int pid, int32_t& ret, std::string& msg);
    std::pair<int, std::string> DealWithDumpCatchRet(int pid, int32_t& ret, std::string& msg);
    void DealAfterPollFail(int pid, std::string& msg, DumpCatcherPipeData& pipeData);
    static int32_t KernelRet2DumpcatchRet(int32_t ret);

private:
    void CollectKernelStackOnFail(int32_t uid, int pid);
    void HandlePollReturnCode(int pollRet, int pid, int32_t& ret, std::string& msg, bool& isPollFail);

    DumpCatcherSharedState& state_;
    TimeoutAnalyzer timeoutAnalyzer_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // RESULT_HANDLER_H
