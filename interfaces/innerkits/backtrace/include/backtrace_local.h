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

/**
 * @brief Get a thread of backtrace frames by specify tid
 *
 * @param frames  backtrace frames(output parameter)
 * @param tid  the id of thread
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @return if succeed return true, otherwise return false
*/
bool GetBacktraceFramesByTid(std::vector<DfxFrame>& frames, int32_t tid, size_t skipFrameNum, bool fast);

/**
 * @brief Get a thread of backtrace string  by specify tid
 *
 * @param out  backtrace string(output parameter)
 * @param tid  the id of thread
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @return if succeed return true, otherwise return false
*/
bool GetBacktraceStringByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast);

/**
 * @brief Print backtrace information to fd
 *
 * @param fd  file descriptor
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @return if succeed return true, otherwise return false
*/
bool PrintBacktrace(int32_t fd = -1, bool fast = false);

/**
 * @brief Get backtrace string of the current process
 *
 * @param out  backtrace string(output parameter)
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @return if succeed return true, otherwise return false
*/
bool GetBacktrace(std::string& out, bool fast = false);

/**
 * @brief Get backtrace string of the current process
 *
 * @param out  backtrace string(output parameter)
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @return if succeed return true, otherwise return false
*/
bool GetBacktrace(std::string& out, size_t skipFrameNum, bool fast = false);

/**
 * @brief Get formated stacktrace string of current process
 *
 * This API is used to get formated stacktrace string of current process
 *
 * @return formated stacktrace string
*/
std::string GetProcessStacktrace();

extern "C" {
    /**
     * @brief Print backtrace information to fd
     *
     * @param fd  file descriptor
     * @return if succeed return true, otherwise return false
    */
    bool PrintTrace(int32_t fd = -1);

    /**
     * @brief Get backtrace of the current process
     *
     * @param skipFrameNum the number of frames to skip
     * @return backtrace of the current process
    */
    const char* GetTrace(size_t skipFrameNum = 0);
}

} // namespace HiviewDFX
} // namespace OHOS
#endif
