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

/* This files contains faultlog fuzzer test modules. */

#include "faultloggerd_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "dfx_dump_catcher.h"
#include "securec.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static const int RAND_TYPE_ONE = 1;
static const int RAND_TYPE_TWO = 2;
static const int RAND_TYPE_TOTAL = 3;
static const int PID_SIZE = 4;
static const int RAND_BUF_LIMIT = 8;
static const int ALPHABET_COUNT = 26;
static const int NUM_COUNT = 10;
static const int RAND_PARAM_SIZE = 20;

namespace OHOS {
char *RandStr(char *str, const int maxLen, const int len)
{
    if (len >= maxLen - 1) {
        return str;
    }
    time_t curTime = time(nullptr);
    if (curTime < 0) {
        exit(1);
    }
    (void)srand(static_cast<unsigned int>(curTime));
    int i;
    for (i = 0; i < len; i++) {
        switch ((rand() % RAND_TYPE_TOTAL)) {
            case RAND_TYPE_ONE:
                str[i] = 'A' + rand() % ALPHABET_COUNT;
                break;
            case RAND_TYPE_TWO:
                str[i] = 'a' + rand() % ALPHABET_COUNT;
                break;
            default:
                str[i] = '0' + rand() % NUM_COUNT;
                break;
        }
    }
    str[++i] = '\0';
    return str;
}

bool DumpStackTraceTest(const uint8_t* data, size_t size)
{
    shared_ptr<DfxDumpCatcher> catcher = make_shared<DfxDumpCatcher>();
    if (size >= RAND_BUF_LIMIT) {
        string msg;
        int pid[1];
        int tid[1];
        errno_t err = memcpy_s(pid, sizeof(pid), data, PID_SIZE);
        if (err != EOK) {
            cout << "DumpStackTraceTest :: memcpy_s pid failed" << endl;
        }
        err = memcpy_s(tid, sizeof(tid), data + PID_SIZE, PID_SIZE);
        if (err != EOK) {
            cout << "DumpStackTraceTest :: memcpy_s tid failed" << endl;
        }
        cout << "pid = " << pid[0] << " tid = " << tid[0] << endl;
        if (catcher->DumpCatch(pid[0], tid[0], msg)) {
            cout << msg << endl;
        } else {
            cout << "DumpStackTraceTest :: dump failed." << endl;
        }

        string processdumpCmd = "processdump -p " + to_string(pid[0]) + " -t " + to_string(tid[0]);
        system(processdumpCmd.c_str());

        char param[RAND_PARAM_SIZE] = { '\0' };
        string processdumpInvalidCmd = "processdump -" + string(RandStr(param, sizeof(param), RAND_BUF_LIMIT)) +
            " " + to_string(pid[0]) + " -t " + to_string(tid[0]);
        system(processdumpInvalidCmd.c_str());
    }
    return true;
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DumpStackTraceTest(data, size);
    return 0;
}
