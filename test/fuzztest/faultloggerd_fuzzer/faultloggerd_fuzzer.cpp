/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "faultloggerd_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include "dfx_dump_catcher.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "securec.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static const int PID_SIZE = 4;
static const int RAND_BUF_LIMIT = 9;

namespace OHOS {
bool DumpStackTraceTest(const uint8_t* data, size_t size)
{
    if (size < RAND_BUF_LIMIT) {
        return true;
    }
    shared_ptr<DfxDumpCatcher> catcher = make_shared<DfxDumpCatcher>();
    string msg;
    int pid[1];
    int tid[1];
    errno_t err = memcpy_s(pid, sizeof(pid), data, PID_SIZE);
    if (err != EOK) {
        cout << "DumpStackTraceTest :: memcpy_s pid failed" << endl;
    }
    data += PID_SIZE;
    err = memcpy_s(tid, sizeof(tid), data, PID_SIZE);
    if (err != EOK) {
        cout << "DumpStackTraceTest :: memcpy_s tid failed" << endl;
    }
    data += PID_SIZE;
    char invalidOption = *data;
    cout << "pid = " << pid[0] << " tid = " << tid[0] << " invalidOpt = " << invalidOption << endl;

    if (catcher->DumpCatch(pid[0], tid[0], msg)) {
        cout << msg << endl;
    } else {
        cout << "DumpStackTraceTest :: dumpcatch failed." << endl;
    }

    string processdumpCmd = "dumpcatcher -p " + to_string(pid[0]) + " -t " + to_string(tid[0]);
    system(processdumpCmd.c_str());

    string processdumpInvalidCmd = "dumpcatcher -" + to_string(invalidOption) + " -p " +
        to_string(pid[0]) + " -t " + to_string(tid[0]);
    system(processdumpInvalidCmd.c_str());

    return true;
}

bool FaultloggerdClientTest(const uint8_t* data, size_t size)
{
    cout << "enter FaultloggerdClientTest, size:" << size << endl;
    if (size < sizeof(int32_t) * 3) { //3: 3 parameters
        cout << "size is not correct, return" << endl;
        return true;
    }
    int32_t type[1];
    int32_t pid[1];
    int32_t tid[1];
    errno_t err = memcpy_s(type, sizeof(type), data, sizeof(int32_t));
    if (err != EOK) {
        cout << "FaultloggerdClientTest :: memcpy_s type failed" << endl;
    }
    data += sizeof(int32_t);
    err = memcpy_s(tid, sizeof(tid), data, sizeof(int32_t));
    if (err != EOK) {
        cout << "FaultloggerdClientTest :: memcpy_s tid failed" << endl;
    }
    data += sizeof(int32_t);
    err = memcpy_s(pid, sizeof(pid), data, sizeof(int32_t));
    if (err != EOK) {
        cout << "FaultloggerdClientTest :: memcpy_s pid failed" << endl;
    }
    cout << "pid = " << pid[0] << " tid = " << tid[0] << " type = " << type[0] << endl;

    RequestFileDescriptor(type[0]);
    RequestPipeFd(pid[0], type[0]);
    RequestDelPipeFd(pid[0]);
    RequestCheckPermission(pid[0]);
    RequestSdkDump(type[0], pid[0], tid[0]);

    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    daemon->HandleRequest(pid[0], type[0]);
    return true;
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::DumpStackTraceTest(data, size);
    OHOS::FaultloggerdClientTest(data, size);
    return 0;
}
