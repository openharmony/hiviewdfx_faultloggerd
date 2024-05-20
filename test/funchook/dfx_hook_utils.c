/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "dfx_hook_utils.h"

#include <securec.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include <libunwind_i.h>

#include "dfx_define.h"
#include "dfx_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxFuncHook"
#endif

#define MIN_FRAME 4
#define MAX_FRAME 64
#define BUF_SZ 512

static pthread_mutex_t g_backtraceLock = PTHREAD_MUTEX_INITIALIZER;

void LogBacktrace(void)
{
    pthread_mutex_lock(&g_backtraceLock);
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    int index = 0;
    unw_word_t pc;
    unw_word_t prevPc;
    bool shouldContinue = true;
    while (true) {
        if (index > MAX_FRAME) {
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&pc))) {
            break;
        }

        if (index > MIN_FRAME && prevPc == pc) {
            break;
        }
        prevPc = pc;
        index++;

        if (!shouldContinue) {
            break;
        }

        int ret = unw_step(&cursor);
        if (ret == 0) {
            shouldContinue = false;
        } else if (ret < 0) {
            break;
        }
    }
    pthread_mutex_unlock(&g_backtraceLock);
}