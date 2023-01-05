/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef BACKTRACE_LOCAL_STATIC_H
#define BACKTRACE_LOCAL_STATIC_H

#include <cinttypes>
#include <csignal>

#include <libunwind.h>
#include <nocopyable.h>

namespace OHOS {
namespace HiviewDFX {
class BacktraceLocalStatic {
public:
    static BacktraceLocalStatic& GetInstance();
    ~BacktraceLocalStatic() = default;
    // should be protected by lock
    static bool GetThreadContext(int32_t tid, unw_context_t& ctx);
    static void ReleaseThread(int32_t tid);

private:
    BacktraceLocalStatic();
    DISALLOW_COPY_AND_MOVE(BacktraceLocalStatic);
    static void CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context);
    static bool InstallSigHandler();
    static void UninstallSigHandler();
    static bool SignalRequestThread(int32_t tid, unw_context_t& ctx);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // BACKTRACE_LOCAL_STATIC_H
