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
#ifndef DFX_SIGNAL_HANDLER_H
#define DFX_SIGNAL_HANDLER_H

#include <inttypes.h>
#include <threads.h>
#include <signal.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief callback function of thread infomation when thread crash
 *
 * @param buf buffer for writing thread infomation
 * @param len length of buffer
 * @param ucontext userlevel context
*/
typedef void(*ThreadInfoCallBack)(char* buf, size_t len, void* ucontext);

/**
 * @brief set callback function of thread infomation
 *
 * @param func  callback function of thread infomation
*/
void SetThreadInfoCallback(ThreadInfoCallBack func);

/**
 * @brief install signal handler
*/
void DFX_InstallSignalHandler(void);

typedef uint64_t(*GetStackIdFunc)(void);
void SetAsyncStackCallbackFunc(void* func);

int DFX_SetAppRunningUniqueId(const char* appRunningId, size_t len);

/**
 * @brief set crash object which is measurement information of crash
 *
 * @param type  type of object, eg 0 represent string type
 * @param addr  addr of object
 * @return return crash Object which set up last time
*/
uintptr_t DFX_SetCrashObj(uint8_t type, uintptr_t addr);

/**
 * @brief reset crash object
 *
 * @param crashObj return of DFX_SetCrashObj
*/
void DFX_ResetCrashObj(uintptr_t crashObj);
#ifdef __cplusplus
}
#endif
#endif
