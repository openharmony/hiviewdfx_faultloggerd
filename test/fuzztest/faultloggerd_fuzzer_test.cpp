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

/* This files contains faultlog fuzzer test modules. */

#include "faultloggerd_fuzzer_test.h"

#include <iostream>
#include <stddef.h>
#include <stdint.h>
#include <cstdlib>
#include "securec.h"
#include <ctime>
#include "dfx_dump_catcher.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static const int NUMBER_ONE = 1;
static const int NUMBER_TWO = 2;
static const int NUMBER_THREE = 3;
static const int NUMBER_FOUR = 4;
static const int NUMBER_EIGHT = 8;
static const int NUMBER_TEN = 10;
static const int NUMBER_TS = 26;


namespace OHOS {
    char *RandStr(char *str, const int MaxLen,  const int len)
    {
        if (len >= MaxLen - 1) {
            return str;
        }
        time_t time = time(nullptr);
        if (time < 0) {
            printf("worry time !");
            exit(1);
        }
        (void)srand(static_cast<unsigned int>time);
        int i;
        for (i = 0; i < len; i++) {
            switch ((rand() % NUMBER_THREE)) {
                case NUMBER_ONE:
                    str[i] = 'A' + rand() % NUMBER_TS;
                    break;
                case NUMBER_TWO:
                    str[i] = 'a' + rand() % NUMBER_TS;
                    break;
                default:
                    str[i] = '0' + rand() % NUMBER_TEN;
                    break;
            }
        }
        str[++i] = '\0';
        return str;
    }

    bool DoSomethingInterestingWithMyAPI(const uint8_t* data, size_t size)
    {
        const int minSize = NUMBER_EIGHT;
        const int psSize = NUMBER_ONE;
        const int idSize = NUMBER_FOUR;
        std::shared_ptr<DfxDumpCatcher> catcher = std::make_shared<DfxDumpCatcher>();
        // invoke need test interface or API
        if (size >= minSize) {
            std::string msg;
            int pid[1];
            int tid[1];
            errno_t err = memcpy_s(pid, sizeof(pid), data, idSize);
            if (err != EOK) {
                std::cout << "DoSomethingInterestingWithMyAPI :: memcpy_s pid failed" << endl;
            }
            err = memcpy_s(tid, sizeof(tid), data + idSize, idSize);
            if (err != EOK) {
                std::cout << "DoSomethingInterestingWithMyAPI :: memcpy_s tid failed" << endl;
            }

            cout << "data = " << *data << endl;
            for (auto i = 0; i < minSize; i++) {
                cout << "data = " << data[i] << endl;
            }
            cout << "pid = " << pid[0] << endl;
            cout << "tid = " << tid[0] << endl;
            catcher->DumpCatch(pid[0], tid[0], msg);
            std::cout << "msg = " << msg << endl;

            std::string processdumpCMD = "processdump -p " + std::to_string(pid[0]) + " -t " + std::to_string(tid[0]);
            char dumpCMD[100];
            err = strcpy_s(dumpCMD, sizeof(dumpCMD), processdumpCMD.c_str());
            if (err != EOK) {
                std::cout << "DoSomethingInterestingWithMyAPI :: strcpy_s failed" << endl;
            }
            system(dumpCMD);

            char param[20] = {'\0'};
            std::string ProcessDumpInvalidCmd = "processdump -" +
                std::string(randstr(param, sizeof(param), NUMBER_EIGHT)) +
                " " + std::to_string(pid[0]) + " -t " + std::to_string(tid[0]);
            system(ProcessDumpInvalidCmd.c_str());
        }
        return true;
    }
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DoSomethingInterestingWithMyAPI(data, size);
    return 0;
}
