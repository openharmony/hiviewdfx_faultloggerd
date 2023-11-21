/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "dump_catcher_demo.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include "dfx_dump_catcher.h"
#include "iosfwd"
#include "ostream"

using namespace std;

static NOINLINE int TestFunc10(void)
{
    OHOS::HiviewDFX::DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(getpid(), gettid(), msg);
    if (ret) {
        cout << msg << endl;
    }
    return 0;
}

static NOINLINE int TestFuncRemote(int32_t pid, int32_t tid)
{
    OHOS::HiviewDFX::DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(pid, tid, msg);
    if (ret) {
        cout << msg << endl;
    }
    return ret;
}

// auto gen function
GEN_TEST_FUNCTION(0, 1)
GEN_TEST_FUNCTION(1, 2)
GEN_TEST_FUNCTION(2, 3)
GEN_TEST_FUNCTION(3, 4)
GEN_TEST_FUNCTION(4, 5)
GEN_TEST_FUNCTION(5, 6)
GEN_TEST_FUNCTION(6, 7)
GEN_TEST_FUNCTION(7, 8)
GEN_TEST_FUNCTION(8, 9)
GEN_TEST_FUNCTION(9, 10)

static bool ParseParameters(int argc, char *argv[], int32_t &pid, int32_t &tid)
{
    switch (argc) {
        case 3:
            if (!strcmp("-p", argv[1])) {
                pid = atoi(argv[2]);
                return true;
            }
            if (!strcmp("-t", argv[1])) {
                pid = getpid();
                tid = atoi(argv[2]);
                return true;
            }
            break;
        case 5:
            if (!strcmp("-p", argv[1])) {
                pid = atoi(argv[2]);

                if (!strcmp("-t", argv[3])) {
                    tid = atoi(argv[4]);
                    return true;
                }
            } else if (!strcmp("-t", argv[1])) {
                tid = atoi(argv[2]);

                if (!strcmp("-p", argv[3])) {
                    pid = atoi(argv[4]);
                    return true;
                }
            }
            break;
        default:
            break;
    }
    return false;
}

int main(int argc, char *argv[])
{
    int32_t pid = 0;
    int32_t tid = 0;
    if (ParseParameters(argc, argv, pid, tid)) {
        TestFuncRemote(pid, tid);
    } else {
        TestFunc0();
    }

    return 0;
}