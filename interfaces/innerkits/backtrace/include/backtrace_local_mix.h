/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef DFX_BACKTRACE_LOCAL_MIX_H
#define DFX_BACKTRACE_LOCAL_MIX_H

#include <cinttypes>
#include <string>
#include <vector>

#include "dfx_define.h"
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {

/**
 * @brief Get a thread of backtrace string  by specify tid
 *
 * @param out  backtrace string(output parameter)
 * @param tid  the id of thread.
 *             notice: the value can not equal to tid of the caller thread,
 *             if you want to get stack of the caller thread,
 *             please use interface of "GetBacktrace" or "PrintBacktrace".
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @param enableKernelStack if set to true, when failed to get user stack, try to get kernel stack.
 * @return if succeed return true, otherwise return false
 * @warning If enableKernelStack set to true,  this interface requires that the caller process
 *          has ioctl system call permission, otherwise it may cause the calling process to crash.
*/
bool GetBacktraceStringByTidWithMix(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM, bool enableKernelStack = true);
} // namespace HiviewDFX
} // namespace OHOS
#endif
