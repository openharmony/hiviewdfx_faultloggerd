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
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_dump_catcher_errno.h"

namespace OHOS {
namespace HiviewDFX {
static const size_t DEFAULT_MAX_FRAME_NUM = 256;
class DfxDumpCatcher {
public:
    DfxDumpCatcher() {}
    ~DfxDumpCatcher() {}

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
    bool DumpCatchMultiPid(const std::vector<int> pidV, std::string& msg);
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
    bool DoDumpCurrTid(const size_t skipFrameNum, std::string& msg, size_t maxFrameNums);
    bool DoDumpLocalTid(const int tid, std::string& msg, size_t maxFrameNums);
    bool DoDumpLocalPid(int pid, std::string& msg, size_t maxFrameNums);
    bool DoDumpLocalLocked(int pid, int tid, std::string& msg, size_t maxFrameNums);
    int32_t DoDumpRemoteLocked(int pid, int tid, std::string& msg, bool isJson = false,
        int timeout = DUMPCATCHER_REMOTE_TIMEOUT);
    int32_t DoDumpCatchRemote(int pid, int tid, std::string& msg, bool isJson = false,
        int timeout = DUMPCATCHER_REMOTE_TIMEOUT);
    int DoDumpRemotePid(int pid, std::string& msg, int (&pipeReadFd)[2],
        bool isJson = false, int32_t timeout = DUMPCATCHER_REMOTE_TIMEOUT);
    int DoDumpRemotePoll(int timeout, std::string& msg, const int pipeReadFd[2], bool isJson = false);
    bool DoReadBuf(int fd, std::string& msg);
    bool DoReadRes(int fd, bool &ret, std::string& msg);
    static void CollectKernelStack(pid_t pid, int waitMilliSeconds = 0);
    void AsyncGetAllTidKernelStack(pid_t pid, int waitMilliSeconds = 0);
    void DealWithPollRet(int pollRet, int pid, int32_t& ret, std::string& msg);
    void DealWithSdkDumpRet(int sdkdumpRet, int pid, int32_t& ret, std::string& msg);

private:
    static const int DUMPCATCHER_REMOTE_P90_TIMEOUT = 1000;
    static const int DUMPCATCHER_REMOTE_TIMEOUT = 10000;
    std::mutex mutex_;
    int32_t pid_ = -1;
    std::string halfProcStatus_ = "";
    std::string halfProcWchan_ = "";
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
