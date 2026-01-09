/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <stack>

#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "decorative_dump_info.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class MapsTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string MapsTest::result = "";
std::set<uintptr_t> interestedAddrs;

void MapsTest::SetUpTestCase(void)
{
    result = "";
}

void MapsTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(MapsTest::WriteLogFunc);
}

int MapsTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    MapsTest::result.append(std::string(buf, len));
    return 0;
}
 
namespace {
void Init(pid_t pid, DfxProcess& process, ProcessDumpRequest& request, Unwinder& unwinder)
{
    pid_t tid = pid;
    pid_t nsPid = pid;
    request.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    request.tid = tid;
    request.pid = pid;
    request.nsPid = pid;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitKeyThread(request);
    auto regs = DfxRegs::CreateRemoteRegs(pid); // can not get context, so create regs self
    process.SetFaultThreadRegisters(regs);
    unwinder.SetRegs(regs);
    unwinder.UnwindRemote(tid, true);
    process.GetKeyThread()->SetFrames(unwinder.GetFrames());
}

std::vector<std::pair<uintptr_t, uintptr_t>> GetAddrFromMaps(std::string& result)
{
    size_t end = result.find('\n');
    size_t start = 0;
    std::stack<uintptr_t> mapsAddr;
    std::string preInode = "";
    while (end != std::string::npos) {
        std::string preLineMap = result.substr(start, end - start);
        size_t mid = preLineMap.find('-');
        if (mid != std::string::npos) {
            // Convert to uintptr_t type
            uintptr_t leftAddr = std::stoll(preLineMap.substr(0, mid), nullptr, 16);
            // Convert to uintptr_t type
            uintptr_t rightAddr = std::stoll(preLineMap.substr(mid + 1, mid), nullptr, 16);
            std::string curInode = preLineMap.substr(preLineMap.length() - 8, 8);
            if (!mapsAddr.empty() && leftAddr == mapsAddr.top() && preInode == curInode) {
                mapsAddr.pop();
                mapsAddr.push(rightAddr);
            } else {
                preInode = curInode;
                mapsAddr.push(leftAddr);
                mapsAddr.push(rightAddr);
            }
        }
        start = end + 1;
        end = result.find('\n', start);
    }
    std::vector<std::pair<uintptr_t, uintptr_t>> deletedMaps;
    while (!mapsAddr.empty()) {
        uintptr_t rightAddr = mapsAddr.top();
        mapsAddr.pop();
        uintptr_t leftAddr = mapsAddr.top();
        mapsAddr.pop();
        std::pair<uintptr_t, uintptr_t> mapsAddrPair = {leftAddr, rightAddr};
        deletedMaps.push_back(mapsAddrPair);
    }
    return deletedMaps;
}

std::set<uintptr_t> InitInterestedAddrs(DfxProcess& process, ProcessDumpRequest& request)
{
    interestedAddrs = process.GetMemoryValues();
    auto regsData = process.GetFaultThreadRegisters()->GetRegsData();
    interestedAddrs.insert(regsData.begin(), regsData.end());
    auto threads = process.GetOtherThreads();
    threads.emplace_back(process.GetKeyThread());
    interestedAddrs.insert(reinterpret_cast<uintptr_t>(request.siginfo.si_addr));
    for (const auto& thread : threads) {
        for (const auto &frame : thread->GetFrames()) {
            interestedAddrs.emplace(frame.pc);
        }
    }
    return interestedAddrs;
}

bool CheckInterestedAddrsValues(const std::pair<uintptr_t, uintptr_t>& pair)
{
    for (const auto& addr : interestedAddrs) {
        if (addr >= pair.first && addr < pair.second) {
            return true;
        }
    }
    return false;
}

std::vector<std::pair<uintptr_t, uintptr_t>> GetDeletedMaps(
    std::vector<std::pair<uintptr_t, uintptr_t>> unSimplifyMaps,
    std::vector<std::pair<uintptr_t, uintptr_t>> simplifyMaps)
{
    std::vector<std::pair<uintptr_t, uintptr_t>> deletedMaps;
    for (size_t i = 0; i < unSimplifyMaps.size(); i++) {
        bool deleted = false;
        for (size_t j = 0; j < simplifyMaps.size(); j++) {
            if (unSimplifyMaps[i].first == simplifyMaps[j].first) {
                deleted = true;
                break;
            }
        }
        if (!deleted) {
            deletedMaps.push_back(unSimplifyMaps[i]);
        }
    }
    return deletedMaps;
}

bool CheckSimplify(std::vector<std::pair<uintptr_t, uintptr_t>>& unSimplifyMaps,
    std::vector<std::pair<uintptr_t, uintptr_t>>& simplifyMaps)
{
    if (unSimplifyMaps.size() == 0 || simplifyMaps.size() == 0) {
        return false;
    }
    for (auto& simplifyMap : simplifyMaps) {
        if (!CheckInterestedAddrsValues(simplifyMap)) {
            GTEST_LOG_(INFO) << "CheckSimplifyMap Failed" << simplifyMap.first << "-" << simplifyMap.second;
            return false;
        }
    }
    auto deletedMaps = GetDeletedMaps(unSimplifyMaps, simplifyMaps);
    for (auto& deletedMap : deletedMaps) {
        for (const auto& addr : interestedAddrs) {
            if (addr >= deletedMap.first && addr < deletedMap.second) {
                GTEST_LOG_(INFO) << "CheckDeletedMap Failed" << deletedMap.first << "-" << deletedMap.second;
                return false;
            }
        }
    }
    return true;
}

/**
 * @tc.name: MapsTest001
 * @tc.desc: test print maps
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, MapsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MapsTest001: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    DfxProcess process;
    ProcessDumpRequest request;
    Unwinder unwinder(pid, pid, true);
    Init(pid, process, request, unwinder);
    Maps maps;
    maps.Print(process, request, unwinder);
    auto unSimplifyMaps = GetAddrFromMaps(result);
    result = "";
    CrashLogConfig crashLogConfig = {
        .simplifyVmaPrinting = true,
    };
    process.SetCrashLogConfig(crashLogConfig);
    InitInterestedAddrs(process, request);
    maps.Print(process, request, unwinder);
    auto simplifyMaps = GetAddrFromMaps(result);
    ASSERT_TRUE(CheckSimplify(unSimplifyMaps, simplifyMaps));
    process.Detach();
    int status;
    waitpid(pid, &status, 0);
    GTEST_LOG_(INFO) << "MapsTest001: end.";
}
}
 