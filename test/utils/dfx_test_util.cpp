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

#include "dfx_test_util.h"

#include <fstream>
#include <sstream>
#include <unistd.h>

#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const int BUF_LEN = 128;
}

std::string ExecuteCommands(const std::string& cmds)
{
    if (cmds.empty()) {
        return "";
    }
    FILE *procFileInfo = nullptr;
    std::string cmdLog;
    procFileInfo = popen(cmds.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        return "Execute failed.";
    }
    char res[BUF_LEN] = { '\0' };
    while (fgets(res, sizeof(res), procFileInfo) != nullptr) {
        cmdLog += res;
    }
    pclose(procFileInfo);
    return cmdLog;
}

int GetProcessPid(const std::string& serviceName)
{
    std::string cmd = "pidof " + serviceName;
    std::string pidStr = ExecuteCommands(cmd);
    int32_t pid = 0;
    std::stringstream pidStream(pidStr);
    pidStream >> pid;
    printf("the pid of process(%s) is %s \n", serviceName.c_str(), pidStr.c_str());
    return pid;
}

int CountLines(const std::string& fileName)
{
    std::ifstream readFile;
    std::string tmpuseValue;
    readFile.open(fileName.c_str(), std::ios::in);
    if (readFile.fail()) {
        return 0;
    } else {
        int n = 0;
        while (getline(readFile, tmpuseValue, '\n')) {
            n++;
        }
        readFile.close();
        return n;
    }
}
} // namespace HiviewDFX
} // namespace OHOS