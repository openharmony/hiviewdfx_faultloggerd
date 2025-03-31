/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <cstdio>
#include <csignal>
#include <cstring>
#include <memory>
#include <pthread.h>
#include <securec.h>
#include "dfx_define.h"
#include "stack_utils.h"

namespace OHOS {
namespace HiviewDFX {
StackUtils::StackUtils()
{
    ParseSelfMaps();
}

StackUtils& StackUtils::Instance()
{
    static StackUtils inst;
    return inst;
}

bool StackUtils::GetMainStackRange(uintptr_t& stackBottom, uintptr_t& stackTop) const
{
    if (!mainStack_.Valid()) {
        return false;
    }
    stackBottom = mainStack_.start;
    stackTop = mainStack_.end;
    return true;
}

bool StackUtils::GetArkStackRange(uintptr_t& start, uintptr_t& end) const
{
    if (!arkCode_.Valid()) {
        return false;
    }
    start = arkCode_.start;
    end = arkCode_.end;
    return true;
}

bool StackUtils::GetSelfStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    pthread_attr_t tattr;
    if (pthread_getattr_np(pthread_self(), &tattr) != 0) {
        return false;
    }
    void *base = nullptr;
    size_t size = 0;
    bool ret = false;
    if (pthread_attr_getstack(&tattr, &base, &size) == 0) {
        stackBottom = reinterpret_cast<uintptr_t>(base);
        stackTop = reinterpret_cast<uintptr_t>(base) + size;
        ret = true;
    }
    pthread_attr_destroy(&tattr);
    return ret;
}

bool StackUtils::GetSigAltStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    stack_t altStack;
    if (sigaltstack(nullptr, &altStack) == -1) {
        return false;
    }
    if ((static_cast<uint32_t>(altStack.ss_flags) & SS_ONSTACK) != 0) {
        stackBottom = reinterpret_cast<uintptr_t>(altStack.ss_sp);
        stackTop = reinterpret_cast<uintptr_t>(altStack.ss_sp) + altStack.ss_size;
        return true;
    }
    return false;
}

void StackUtils::ParseSelfMaps()
{
    std::unique_ptr<FILE, void (*)(FILE*)> fp(fopen(PROC_SELF_MAPS_PATH, "r"), [](FILE* fp) {
        if (fp != nullptr) {
            fclose(fp);
        }
    });
    if (fp == nullptr) {
        return;
    }
    auto parser = [] (const char *mapInfo, bool checkExec, MapRange &range) {
        uint64_t begin = 0;
        uint64_t end = 0;
        uint64_t offset = 0;
        char perms[5] = {0}; // 5:rwxp
        int pos = 0;
        if (sscanf_s(mapInfo, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
            &perms, sizeof(perms), &offset, &pos) != 4) { // 4:scan size
            return;
        }
        if (checkExec && strcmp(perms, "r-xp") != 0) {
            return;
        }
        range.start = begin;
        range.end = end;
    };

    char mapInfo[256] = {0}; // 256: map info size
    while (fgets(mapInfo, sizeof(mapInfo), fp.get()) != nullptr) {
        if (mainStack_.Valid() && arkCode_.Valid()) {
            return;
        }
        if (!mainStack_.Valid() && strstr(mapInfo, "[stack]") != nullptr) {
            parser(mapInfo, false, mainStack_);
            continue;
        }

        if (!arkCode_.Valid() && strstr(mapInfo, "stub.an") != nullptr) {
            parser(mapInfo, true, arkCode_);
        }
    }
}
} // namespace HiviewDFX
} // namespace OHOS
