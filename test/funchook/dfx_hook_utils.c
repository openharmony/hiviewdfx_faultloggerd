/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <libunwind_i-ohos.h>
#include <map_info.h>

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
    unw_addr_space_t as = NULL;
    unw_init_local_address_space(&as);
    if (as == NULL) {
        pthread_mutex_unlock(&g_backtraceLock);
        return;
    }

    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local_with_as(as, &cursor, &context);

    int index = 0;
    unw_word_t pc;
    unw_word_t relPc;
    unw_word_t prevPc;
    unw_word_t sz;
    uint64_t start;
    uint64_t end;
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

        relPc = unw_get_rel_pc(&cursor);
        struct map_info* mapInfo = unw_get_map(&cursor);
        if (mapInfo == NULL && index > 1) {
            break;
        }

        sz = unw_get_previous_instr_sz(&cursor);
        if (index != 0 && relPc > sz) {
            relPc -= sz;
            pc -= sz;
        }

        char buf[BUF_SZ];
        (void)memset_s(&buf, sizeof(buf), 0, sizeof(buf));
        if (unw_get_symbol_info_by_pc(as, pc, BUF_SZ, buf, &start, &end) == 0) {
            LOGI("#%02d %016p(%016p) %s(%s)\n", index, relPc, pc,
                mapInfo == NULL ? "Unknown" : mapInfo->path,
                buf);
        } else {
            LOGI("#%02d %016p(%016p) %s\n", index, relPc, pc,
                mapInfo == NULL ? "Unknown" : mapInfo->path);
        }
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
    unw_destroy_local_address_space(as);
    pthread_mutex_unlock(&g_backtraceLock);
}