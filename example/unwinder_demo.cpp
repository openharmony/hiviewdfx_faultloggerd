/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "dfx_ptrace.h"
#include "unwinder.h"

static const int IDX_ONE = 1;
static const int IDX_TWO = 2;
static const int IDX_THREE = 3;
static const int IDX_FOUR = 4;
static const int IDX_FIVE = 5;
using namespace OHOS::HiviewDFX;

static bool ParseParameters(int argc, char *argv[], int32_t &pid, int32_t &tid)
{
    switch (argc) {
        case IDX_THREE:
            if (!strcmp("-p", argv[IDX_ONE])) {
                pid = atoi(argv[IDX_TWO]);
                tid = pid;
                return true;
            }
            if (!strcmp("-t", argv[IDX_ONE])) {
                pid = getpid();
                tid = atoi(argv[IDX_TWO]);
                return true;
            }
            break;
        case IDX_FIVE:
            if (!strcmp("-p", argv[IDX_ONE])) {
                pid = atoi(argv[IDX_TWO]);

                if (!strcmp("-t", argv[IDX_THREE])) {
                    tid = atoi(argv[IDX_FOUR]);
                    return true;
                }
            } else if (!strcmp("-t", argv[IDX_ONE])) {
                tid = atoi(argv[IDX_TWO]);

                if (!strcmp("-p", argv[IDX_THREE])) {
                    pid = atoi(argv[IDX_FOUR]);
                    return true;
                }
            }
            break;
        default:
            perror("argc error\n");
            break;
    }
    return false;
}

int main(int argc, char *argv[])
{
    int32_t pid = 0;
    int32_t tid = 0;
    if (!ParseParameters(argc, argv, pid, tid)) {
        return -1;
    }

    do {
        printf("pid: %d, tid: %d\n", pid, tid);
        if (!DfxPtrace::Attach(pid)) {
            printf("Failed to attach pid: %d\n", pid);
            break;
        }
        if ((pid != tid) && !DfxPtrace::Attach(tid)) {
            printf("Failed to attach tid: %d\n", tid);
            break;
        }

        auto unwinder = std::make_shared<Unwinder>(pid);
        if (!unwinder->UnwindRemote(tid)) {
            printf("Failed to unwind tid: %d\n", tid);
            break;
        }
        auto frames = unwinder->GetFrames();
        printf("frames:\n");
        printf("%s\n", Unwinder::GetFramesStr(frames).c_str());
    } while (false);
    if (pid != tid) {
        DfxPtrace::Detach(tid);
    }
    DfxPtrace::Detach(pid);
    return 0;
}
