/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef STACK_UTIL_H
#define STACK_UTIL_H

#include <cstdio>
#include <csignal>
#include <cstring>
#include <string>
#include <pthread.h>
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static uintptr_t g_stackMapStart = 0;
static uintptr_t g_stackMapEnd = 0;
static uintptr_t g_arkMapStart = 0;
static uintptr_t g_arkMapEnd = 0;

static bool ParseSelfMaps()
{
    FILE *fp = fopen(PROC_SELF_MAPS_PATH, "r");
    if (fp == NULL) {
        return false;
    }
    bool ret = false;
    char mapInfo[256] = {0}; // 256: map info size
    int pos = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    char perms[5] = {0}; // 5:rwxp
    while (fgets(mapInfo, sizeof(mapInfo), fp) != NULL) {
        if (strstr(mapInfo, "[stack]") != NULL) {
            if (sscanf_s(mapInfo, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
                &perms, sizeof(perms), &offset, &pos) != 4) { // 4:scan size
                continue;
            }
            g_stackMapStart = static_cast<uintptr_t>(begin);
            g_stackMapEnd = static_cast<uintptr_t>(end);
            ret = true;
        }

        if (strstr(mapInfo, "stub.an") != NULL) {
            if (sscanf_s(mapInfo, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
                &perms, sizeof(perms), &offset, &pos) != 4) { // 4:scan size
                continue;
            }
            g_arkMapStart = static_cast<uintptr_t>(begin);
            g_arkMapEnd = static_cast<uintptr_t>(end);
            ret = true;
        }
    }
    (void)fclose(fp);
    return ret;
};
}

AT_UNUSED inline bool GetArkStackRange(uintptr_t& start, uintptr_t& end)
{
    if (g_arkMapStart == 0 || g_arkMapEnd == 0) {
        ParseSelfMaps();
    }
    start = g_arkMapStart;
    end = g_arkMapEnd;
    return (g_arkMapStart != 0 && g_arkMapEnd != 0);
}

AT_UNUSED inline bool GetMainStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (g_stackMapStart == 0 || g_stackMapEnd == 0) {
        ParseSelfMaps();
    }
    stackBottom = g_stackMapStart;
    stackTop = g_stackMapEnd;
    return (g_stackMapStart != 0 && g_stackMapEnd != 0);
}

AT_UNUSED static bool GetSelfStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    bool ret = false;
    pthread_attr_t tattr;
    void *base = nullptr;
    size_t size = 0;
    if (pthread_getattr_np(pthread_self(), &tattr) != 0) {
        return ret;
    }
    if (pthread_attr_getstack(&tattr, &base, &size) == 0) {
        stackBottom = reinterpret_cast<uintptr_t>(base);
        stackTop = reinterpret_cast<uintptr_t>(base) + size;
        ret = true;
    }
    pthread_attr_destroy(&tattr);
    return ret;
}

AT_UNUSED static bool GetSigAltStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    bool ret = false;
    stack_t altStack;
    if (sigaltstack(nullptr, &altStack) != -1) {
        if ((static_cast<uint32_t>(altStack.ss_flags) & SS_ONSTACK) != 0) {
            stackBottom = reinterpret_cast<uintptr_t>(altStack.ss_sp);
            stackTop = reinterpret_cast<uintptr_t>(altStack.ss_sp) + altStack.ss_size;
            ret = true;
        }
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif