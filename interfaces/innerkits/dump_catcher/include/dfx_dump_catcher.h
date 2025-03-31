/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_DUMPCATCH_H
#define DFX_DUMPCATCH_H

#include <cinttypes>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
// some module head files not self-included.
#include <unistd.h>
#include <sys/syscall.h>
namespace OHOS {
namespace HiviewDFX {
class DfxDumpCatcher {
public:
    static constexpr size_t DEFAULT_MAX_FRAME_NUM = 256;

    DfxDumpCatcher();
    DfxDumpCatcher(const DfxDumpCatcher&) = delete;
    DfxDumpCatcher& operator=(const DfxDumpCatcher&) = delete;

    /**
     * @brief Dump native stack by specify pid and tid
     *
     * @param pid  process id
     * @param tid  thread id
     * @param msg  message of native stack
     * @param maxFrameNums the maximum number of frames to dump, if pid is not equal to caller pid then it is ignored
     * @param isJson whether message of native stack is json formatted
     * @return if succeed return true, otherwise return false
    */
    bool DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM,
                   bool isJson = false);

    /**
     * @brief Dump native stack by specify pid and tid to file
     *
     * @param pid  process id
     * @param tid  thread id
     * @param fd  file descriptor
     * @param maxFrameNums the maximum number of frames to dump,
     *  if pid is not equal to caller pid then it does not support setting
     * @return if succeed return true, otherwise return false
    */
    bool DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM);

    /**
     * @brief Dump native stack by multi-pid
     *
     * @param pid  process id
     * @param tid  thread id
     * @param msg  message of native stack
     * @return if succeed return true, otherwise return false
    */
    bool DumpCatchMultiPid(const std::vector<int> &pidV, std::string& msg);
    /**
     * @brief Dump stack of process
     *
     * @param pid  process id
     * @param msg  message of stack
     * @param maxFrameNums the maximum number of frames to dump,
     *  if pid is not equal to caller pid then it does not support setting
     * @param isJson whether message of stack is json formatted
     * @return -1: dump catch failed 0:msg is normal stack 1:msg is kernel stack(not json format)
    */
    int DumpCatchProcess(int pid, std::string& msg, size_t maxFrameNums = DEFAULT_MAX_FRAME_NUM,
        bool isJson = false);
    /**
     * @brief Dump stack of process with timeout
     *
     * @param pid  process id
     * @param msg  message of stack
     * @param timeout  Set the dump timeout time to be at least 1000ms
     * @param isJson  whether message of stack is json formatted
     * @return ret and reason.
     *  ret: -1: dump catch failed 0:msg is normal stack 1:msg is kernel stack(not json format)
     *  reason: if ret is 1, it contains normal stack fail reason.
     *          if ret is -1, it contains normal stack fail reason and kernel stack fail reason.
    */
    std::pair<int, std::string> DumpCatchWithTimeout(int pid, std::string& msg, int timeout = 3000,
        int tid = 0, bool isJson = false);
private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
