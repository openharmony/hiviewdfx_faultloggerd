/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "dfx_kernel_stack.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <regex>
#include <string_ex.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dfx_log.h"
#include "dfx_trace_dlsym.h"
#include "elapsed_time.h"
#include "proc_util.h"
#include "smart_fd.h"

#define LOGGER_GET_STACK    _IO(0xAB, 9)
#define LOGGER_GET_STACK_ARKTS    _IO(0xAB, 10)
namespace OHOS {
namespace HiviewDFX {
namespace {
// keep sync with defines in kernel/hicollie
const char* const BBOX_PATH = "/dev/bbox";
const int BUFF_STACK_SIZE = 20 * 1024;
const uint32_t MAGIC_NUM = 0x9517;
MAYBE_UNUSED const int MAX_ALL_FRAME_PARSE_TIME = 3000;
typedef struct HstackVal {
    uint32_t magic {0};
    pid_t pid {0};
    char hstackLogBuff[BUFF_STACK_SIZE] {0};
} HstackVal;
}
int32_t DfxGetKernelStack(int32_t pid, std::string& kernelStack, bool needArkts)
{
    auto kstackBuf = std::make_shared<HstackVal>();
    if (kstackBuf == nullptr) {
        DFXLOGW("Failed create HstackVal, pid:%{public}d, errno:%{public}d", pid, errno);
        return KERNELSTACK_ECREATE;
    }
    kstackBuf->pid = pid;
    kstackBuf->magic = MAGIC_NUM;
    SmartFd fd(open(BBOX_PATH, O_WRONLY | O_CLOEXEC));
    if (!fd) {
        DFXLOGW("Failed to open bbox, pid:%{public}d, errno:%{public}d", pid, errno);
        return KERNELSTACK_EOPEN;
    }
    int ctlCode = needArkts ? LOGGER_GET_STACK_ARKTS : LOGGER_GET_STACK;
    int ret = ioctl(fd.GetFd(), ctlCode, kstackBuf.get());
    int32_t res = KERNELSTACK_ESUCCESS;
    if (ret != 0) {
        DFXLOGW("Failed to get pid(%{public}d) kernel stack, errno:%{public}d", pid, errno);
        res = KERNELSTACK_EIOCTL;
    } else {
        kernelStack = std::string(kstackBuf->hstackLogBuff);
    }
    return res;
}

bool FormatThreadKernelStack(const std::string& kernelStack, DfxThreadStack& threadStack,
    DfxOfflineParser *parser)
{
#ifdef __aarch64__
    DFX_TRACE_SCOPED_DLSYM("FormatThreadKernelStack");
    std::regex headerPattern(R"(name=(.{1,20}), tid=(\d{1,10}), ([\w\=\.]{1,256}, ){3}pid=(\d{1,10}))");
    std::smatch result;
    if (!regex_search(kernelStack, result, headerPattern)) {
        DFXLOGI("search thread name failed");
        return false;
    }
    threadStack.threadName = result[1].str();
    int base {10};
    threadStack.tid = strtol(result[2].str().c_str(), nullptr, base); // 2 : second of searched element
    auto pos = kernelStack.rfind("pid=" + result[result.size() - 1].str() + "):");
    if (pos == std::string::npos) {
        return false;
    }
    std::regex threadStatReg(
        R"(state=.{0,16}, utime=\d{0,64}, stime=\d{0,64}, priority=[0-9-]{0,64}, nice=[0-9-]{0,64}, clk=\d{0,64})");
    if (regex_search(kernelStack, result, threadStatReg)) {
        threadStack.threadStat = result[0].str();
    } else {
        DFXLOGW("search thread stat failed");
    }
    size_t index = 0;
    std::regex framePattern(R"(\[(\w{16})\]\<[\w\?+/]{1,1024}\> \(([\w\-./]{1,1024})\))");
    for (std::sregex_iterator it = std::sregex_iterator(kernelStack.begin() + pos, kernelStack.end(), framePattern);
        it != std::sregex_iterator(); ++it) {
        if ((*it)[2].str().rfind(".elf") != std::string::npos) { // 2 : second of searched element is map name
            continue;
        }
        DfxFrame frame;
        frame.index = index++;
        base = 16; // 16 : Hexadecimal
        frame.relPc = strtoull((*it)[1].str().c_str(), nullptr, base);
        frame.mapName = (*it)[2].str(); // 2 : second of searched element is map name
        if (parser) {
            parser->ParseSymbolWithFrame(frame);
        }
        threadStack.frames.emplace_back(frame);
    }
    return true;
#else
    return false;
#endif
}

bool FormatProcessKernelStack(const std::string& kernelStack, std::vector<DfxThreadStack>& processStack,
    bool needParseSymbol, const std::string& bundleName, bool onlyParseBuildId)
{
#if !defined(is_ohos_lite) && defined(__aarch64__)
    ElapsedTime counter;
    std::vector<std::string> threadKernelStackVec;
    std::string keyWord = "Thread info:";
    OHOS::SplitStr(kernelStack, keyWord, threadKernelStackVec);
    if (threadKernelStackVec.size() == 1 && kernelStack.find(keyWord) == std::string::npos) {
        DFXLOGE("Invalid kernelStack, please check it!");
        return false;
    }
    DfxEnableTraceDlsym(true);
    std::unique_ptr<DfxOfflineParser> parser = nullptr;
    if (needParseSymbol) {
        parser = std::make_unique<DfxOfflineParser>(bundleName, onlyParseBuildId);
    }
    for (const std::string& threadKernelStack : threadKernelStackVec) {
        DfxThreadStack threadStack;
        if (FormatThreadKernelStack(threadKernelStack, threadStack, parser.get())) {
            processStack.emplace_back(threadStack);
        }
    }
    DfxEnableTraceDlsym(false);
    auto costTime = counter.Elapsed<std::chrono::milliseconds>();
    DFXLOGI("format kernel stack cost time = %{public}" PRId64 " ms", costTime);
    if (costTime > MAX_ALL_FRAME_PARSE_TIME || onlyParseBuildId) {
        ReportData reportData;
        reportData.parseCostType = onlyParseBuildId ? ParseCostType::PARSE_ALL_BUILDID_TIME
            : ParseCostType::PARSE_ALL_FRAME_TIME;
        reportData.bundleName = bundleName;
        reportData.costTime = static_cast<uint32_t>(costTime);
        reportData.threadCount = static_cast<uint32_t>(threadKernelStackVec.size());
        DfxOfflineParser::ReportDumpStats(reportData);
    }
    return true;
#else
    return false;
#endif
}
}
}
