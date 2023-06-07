/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_BACKTRACE_LOCAL_H
#define DFX_BACKTRACE_LOCAL_H

#include <cinttypes>
#include <string>
#include <vector>
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {

bool GetBacktraceFramesByTid(std::vector<DfxFrame>& frames, int32_t tid, size_t skipFrameNum, bool fast);
bool GetBacktraceStringByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast);

bool PrintBacktrace(int32_t fd = -1, bool fast = false);
bool GetBacktrace(std::string& out, bool fast = false);
bool GetBacktrace(std::string& out, size_t skipFrameNum, bool fast = false);

std::string GetProcessStacktrace();

extern "C" bool PrintTrace(int32_t fd = -1);
extern "C" const char* GetTrace(size_t skipFrameNum = 0);

} // namespace HiviewDFX
} // namespace OHOS
#endif
