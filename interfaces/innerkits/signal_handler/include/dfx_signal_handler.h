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

const char* DFX_GetAppRunningUniqueId(void);

int DFX_SetAppRunningUniqueId(const char* appRunningId, size_t len);

enum CrashObjType : uint8_t {
    /* string type */
    OBJ_STRING = 0,
    /* 64 byte memory */
    OBJ_MEMORY_64B,
    /* 256 byte memory */
    OBJ_MEMORY_256B,
    /* 1024 byte memory */
    OBJ_MEMORY_1024B,
    /* 2048 byte memory */
    OBJ_MEMORY_2048B,
    /* 4096 byte memory */
    OBJ_MEMORY_4096B,
};
/**
 * @brief set crash object which is measurement information of crash
 *
 * @param type  type of object, using enum CrashObjType
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

enum CrashLogConfigType : uint8_t {
    EXTEND_PRINT_PC_LR = 0,
    CUT_OFF_LOG_FILE,
    SIMPLIFY_PRINT_MAPS,
};
/**
 * @brief set crash log config through HiAppEvent
 *
 * @param type  type of config attribute, using enum CrashLogConfigType
 * @param value value of config attribute
 * @return if succeed return 0, otherwise return -1. The reason for the failure can be found in 'errno'
 * @warning this interface is non-thread safety and signal safety
*/
int DFX_SetCrashLogConfig(uint8_t type, uint32_t value);
#ifdef __cplusplus
}
#endif
#endif
