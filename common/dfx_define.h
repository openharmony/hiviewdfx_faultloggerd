/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef DFX_DEFINE_H
#define DFX_DEFINE_H

#define BOOL int
#define TRUE 1
#define FALSE 0
#define NAME_LEN 128

#if defined(__arm__)
#define USER_REG_NUM        16
#define REG_PC_NUM          15
#define REG_LR_NUM          14
#elif defined(__aarch64__)
#define USER_REG_NUM        34
#define REG_PC_NUM          32
#define REG_LR_NUM          30
#elif defined(__x86_64__)
#define USER_REG_NUM        27
#define REG_PC_NUM          16
#endif

#define ARM_EXEC_STEP_NORMAL        4
#define ARM_EXEC_STEP_THUMB         3

#define CONF_LINE_SIZE 1024
#define CONF_KEY_SIZE 256
#define CONF_VALUE_SIZE 256

#define FAULTSTACK_ITEM_BUFFER_LENGTH 56
#define FAULTSTACK_SP_REVERSE 3
#define FAULTSTACK_FIRST_FRAME_SEARCH_LENGTH 64

#if defined(TEMP_FAILURE_RETRY)
#undef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp)            \
    ({                                     \
    long int _rc;                          \
    do {                                   \
        _rc = (long int)(exp);             \
    } while ((_rc == -1) && (errno == EINTR)); \
    _rc;                                   \
    })
#endif

#endif