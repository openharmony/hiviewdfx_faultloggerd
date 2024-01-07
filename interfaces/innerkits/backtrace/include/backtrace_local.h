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

#include "dfx_define.h"
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
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool GetBacktraceFramesByTid(std::vector<DfxFrame>& frames, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

/**
 * @brief Get a thread of backtrace string  by specify tid
 *
 * @param out  backtrace string(output parameter)
 * @param tid  the id of thread
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool GetBacktraceStringByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

#ifndef is_ohos_lite
/**
 * @brief Get a thread of backtrace json  by specify tid
 *
 * @param out  backtrace string(output parameter)
 * @param tid  the id of thread
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool GetBacktraceJsonByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
    size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);
#endif

/**
 * @brief Print backtrace information to fd
 *
 * @param fd  file descriptor
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool PrintBacktrace(int32_t fd = -1, bool fast = false, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

/**
 * @brief Get backtrace string of the current process
 *
 * @param out  backtrace string(output parameter)
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool GetBacktrace(std::string& out, bool fast = false, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

/**
 * @brief Get backtrace string of the current process
 *
 * @param out  backtrace string(output parameter)
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool GetBacktrace(std::string& out, size_t skipFrameNum, bool fast = false,
                  size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM, bool isJson = false);

/**
 * @brief Get formatted stacktrace string of current process
 *
 * This API is used to get formatted stacktrace string of current process
 *
 * @param maxFrameNums the maximum number of frames to backtrace
 * @param isJson whether message of native stack is json formatted
 * @return formatted stacktrace string
*/
std::string GetProcessStacktrace(size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM, bool isJson = false);

extern "C" {
    /**
     * @brief Print backtrace information to fd
     *
     * @param fd  file descriptor
     * @param maxFrameNums the maximum number of frames to backtrace
     * @return if succeed return true, otherwise return false
    */
    bool PrintTrace(int32_t fd = -1, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

    /**
     * @brief Get backtrace of the current process
     *
     * @param skipFrameNum the number of frames to skip
     * @param maxFrameNums the maximum number of frames to backtrace
     * @return backtrace of the current process
    */
    const char* GetTrace(size_t skipFrameNum = 0, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);
}

} // namespace HiviewDFX
} // namespace OHOS
#endif
