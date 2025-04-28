/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
bool GetBacktraceStringByTid(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM, bool enableKernelStack = true);

/**
 * @brief Get a thread of backtrace string  by specify tid enable mix
 *
 * @param out  backtrace string(output parameter)
 * @param tid  the id of thread
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
 * @brief Get backtrace string of the current thread
 *
 * @param out  backtrace string(output parameter)
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false.
*/
bool GetBacktrace(std::string& out, bool fast = false, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

/**
 * @brief Get backtrace string of the current thread
 *
 * @param out  backtrace string(output parameter)
 * @param skipFrameNum the number of frames to skip
 * @param fast flag for using fp backtrace(true) or dwarf backtrace(false)
 * @param maxFrameNums the maximum number of frames to backtrace
 * @return if succeed return true, otherwise return false
*/
bool GetBacktrace(std::string& out, size_t skipFrameNum, bool fast = false,
                  size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

/**
 * @brief Get formatted stacktrace string of current process
 *
 * This API is used to get formatted stacktrace string of current process
 *
 * @param maxFrameNums the maximum number of frames to backtrace
 * @param enableKernelStack if set to true, when failed to get user stack, try to get kernel stack.
 * @return formatted stacktrace string
 * @warning If enableKernelStack set to true,  this interface requires that the caller process
 *          has ioctl system call permission, otherwise it may cause the calling process to crash.
*/
std::string GetProcessStacktrace(size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM, bool enableKernelStack = true);

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
