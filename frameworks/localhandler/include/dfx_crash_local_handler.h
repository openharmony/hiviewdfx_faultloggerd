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
#ifndef DFX_CRASH_LOCAL_HANDLER_H
#define DFX_CRASH_LOCAL_HANDLER_H

#include <signal.h>
#include <sys/ucontext.h>
#include "dfx_dump_request.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief handle processdump crash
 *   we may fail to unwind in processdump in some circumstances, such as processdump crash or execve timeout
 *   in such cases, we should try unwind in signal handler rather than lost a crash event
 *   only use for collecting crash signal
 * @param request info about process dump request
*/
void CrashLocalHandler(struct ProcessDumpRequest* request);

/**
 * @brief handle processdump crash and print log to file
 *
 * @param fd file descriptor
 * @param request info about process dump request
*/
void CrashLocalHandlerFd(const int fd, struct ProcessDumpRequest* request);
#ifdef __cplusplus
}
#endif
#endif
