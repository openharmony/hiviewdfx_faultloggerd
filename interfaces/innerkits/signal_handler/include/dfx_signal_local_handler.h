/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef DFX_SIGNAL_LOCAL_HANDLER_H
#define DFX_SIGNAL_LOCAL_HANDLER_H

#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>
#include "dfx_signal_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SignalHandlerFunc) (int, siginfo_t *, void *);
void DFX_SetSignalHandlerFunc(SignalHandlerFunc func);

void DFX_InitDumpRequest(struct ProcessDumpRequest* request, const int sig);

void DFX_InstallLocalSignalHandler(void);

#ifdef __cplusplus
}
#endif
#endif
