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

#include <benchmark/benchmark.h>
#ifdef LOCK_TO_CPU
#include <limits.h>
#include <sched.h>
#include <string>
#include <vector>
#include "dfx_log.h"
#include "string_util.h"
#endif

using namespace OHOS::HiviewDFX;
using namespace std;

#ifdef LOCK_TO_CPU
static bool LockToCpu(int lockCpu)
{
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(lockCpu, &cpuSet);
    if (sched_setaffinity(0, sizeof(cpuSet), &cpuSet) != 0) {
        if (errno == EINVAL) {
            DFXLOGW("Invalid cpu %{public}d", lockCpu);
        } else {
            DFXLOGE("sched_setaffinity failed");
        }
        return false;
    }

    DFXLOGI("Locked to cpu %{public}d\n", lockCpu);
    return true;
}

int main(int argc, char** argv)
{
    std::vector<char*> newArgv;
    newArgv.push_back(argv[0]);
    // Look for the special option --benchmark_lock_cpu.
    int lockCpu = -1;
    for (int i = 1; i < argc; i++) {
        if (StartsWith(argv[i], "--benchmark_cpu=")) {
            char* endptr = nullptr;
            long cpu = strtol(&argv[i][16], &endptr, 10);
            if (endptr == nullptr || *endptr != '\0' || cpu > INT_MAX || cpu < 0) {
                DFXLOGW("Malformed value for --benchmark_cpu, requires a valid positive number.");
                return 1;
            }
            lockCpu = cpu;
        } else {
            newArgv.push_back(argv[i]);
        }
    }
    newArgv.push_back(nullptr);
    if (lockCpu != -1 && !LockToCpu(lockCpu)) {
        return 1;
    }

    int newArgc = newArgv.size() - 1;
    ::benchmark::Initialize(&newArgc, newArgv.data());
    if (::benchmark::ReportUnrecognizedArguments(newArgc, newArgv.data())) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
}

#else

BENCHMARK_MAIN();
#endif