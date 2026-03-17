/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "dfx_json_formatter.h"

#include <cstdlib>
#include <cerrno>
#include <chrono>
#include <future>
#include <thread>
#include <sstream>
#include <securec.h>
#include "dfx_kernel_stack.h"
#ifndef is_ohos_lite
#include "dfx_frame_formatter.h"
#include "dfx_offline_parser.h"
#include "dfx_trace_dlsym.h"
#include "json/json.h"
#endif
#include "dfx_log.h"
#include "dfx_util.h"
#include "elapsed_time.h"
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

namespace OHOS {
namespace HiviewDFX {
#ifndef is_ohos_lite
namespace {
struct ParseSymbolOptions {
    bool needParseSymbol {false};
    std::string bundleName {""};
    bool onlyParseBuildId {false};

    ParseSymbolOptions(bool parseSymbol, const std::string& bundle, bool parseBuildId = false)
        : needParseSymbol(parseSymbol), bundleName(bundle), onlyParseBuildId(parseBuildId) {}
};
const int FRAME_BUF_LEN = 1024;
MAYBE_UNUSED const int ALARM_TIME_S = 20;
MAYBE_UNUSED const int WAITPID_TIMEOUT_MS = 3000;
MAYBE_UNUSED const int MAX_ALL_FRAME_PARSE_TIME_MS = 3000;
MAYBE_UNUSED const int PARSE_SO_SYMBOL_REMAIN_TIME_MS = 5000;
MAYBE_UNUSED const int LESS_REMAIN_TIME_MS = 500;
constexpr const char* CONNECTION_STACK = ", out stack string:";
static std::string JsonAsString(const Json::Value& val)
{
    if (val.isConvertibleTo(Json::stringValue)) {
        return val.asString();
    }
    return "";
}

class StackJsonFormatter {
public:
    StackJsonFormatter(const std::string& bundleName, bool onlyParseBuildId)
        : parser_(bundleName, onlyParseBuildId), onlyParseBuildId_(onlyParseBuildId) {}
    bool FormatJsonStack(Json::Value& threads, std::string& outStackStr, std::string& outStackJson);

private:
    bool FormatJsonThreadStack(Json::Value& thread, std::string& outStackStr);
    bool FormatNativeFrame(Json::Value& frames, const uint32_t& frameIdx, std::string& outStr);
    bool FormatJsFrame(const Json::Value& frames, const uint32_t& frameIdx, std::string& outStr);
    bool IsStatInfoValid(Json::Value& thread);

    DfxOfflineParser parser_;
    bool onlyParseBuildId_;
};


bool StackJsonFormatter::FormatJsFrame(const Json::Value& frames, const uint32_t& frameIdx, std::string& outStr)
{
    const int jsIdxLen = 10;
    char buf[jsIdxLen] = { 0 };
    char idxFmt[] = "#%02u at";
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, idxFmt, frameIdx) <= 0) {
        return false;
    }
    outStr = std::string(buf);
    std::string symbol = JsonAsString(frames[frameIdx]["symbol"]);
    if (!symbol.empty()) {
        outStr.append(" " + symbol);
    }
    std::string packageName = JsonAsString(frames[frameIdx]["packageName"]);
    if (!packageName.empty()) {
        outStr.append(" " + packageName);
    }
    std::string file = JsonAsString(frames[frameIdx]["file"]);
    if (!file.empty()) {
        std::string line = JsonAsString(frames[frameIdx]["line"]);
        std::string column = JsonAsString(frames[frameIdx]["column"]);
        outStr.append(" (" + file + ":" + line + ":" + column + ")");
    }
    return true;
}

bool StackJsonFormatter::FormatNativeFrame(Json::Value& frames, const uint32_t& frameIdx, std::string& outStr)
{
    char buf[FRAME_BUF_LEN] = {0};
    char format[] = "#%02u pc %s %s";

    DfxFrame frame;
    frame.index = frameIdx;
    auto pc = JsonAsString(frames[frameIdx]["pc"]);
    constexpr int hexadecimal = 16;
    frame.relPc = strtoull(pc.c_str(), nullptr, hexadecimal);
    frame.mapName = JsonAsString(frames[frameIdx]["file"]);
    frame.buildId = JsonAsString(frames[frameIdx]["buildId"]);
    if (frames[frameIdx].isMember("offset") && frames[frameIdx]["offset"].isUInt64()) {
        frame.funcOffset = frames[frameIdx]["offset"].asUInt64();
    }
    frame.funcName = JsonAsString(frames[frameIdx]["symbol"]);
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frameIdx, pc.c_str(),
                   frame.mapName.empty() ? "Unknown" : frame.mapName.c_str()) <= 0) {
        return false;
    }
    if ((onlyParseBuildId_ && frame.buildId.empty()) ||
        (!onlyParseBuildId_ && frame.funcName.empty())) {
        if (parser_.ParseSymbolWithFrame(frame)) {
            frames[frameIdx]["buildId"] = frame.buildId;
            frames[frameIdx]["offset"] = frame.funcOffset;
            frames[frameIdx]["symbol"] = frame.funcName;
        }
    }
    outStr = std::string(buf);
    if (!frame.funcName.empty()) {
        outStr.append("(" + frame.funcName + "+" + std::to_string(frame.funcOffset) + ")");
    }
    if (!frame.buildId.empty()) {
        outStr.append("(" + frame.buildId + ")");
    }
    return true;
}

static void SafeDelayOneMillSec(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; // 1000000 : 1ms
    OHOS_TEMP_FAILURE_RETRY(nanosleep(&ts, &ts));
}

static int WaitProcessExitTimeout(pid_t pid, int timeoutMs)
{
    int status = 1; // abnomal exit code
    while (timeoutMs > 0) {
        int res = waitpid(pid, &status, WNOHANG);
        if (res > 0) {
            break;
        } else if (res < 0) {
            DFXLOGE("failed to wait processdump_parser, error(%{public}d)", errno);
            break;
        }
        SafeDelayOneMillSec();
        timeoutMs--;
        if (timeoutMs == 0) {
            DFXLOGI("waitpid %{public}d timeout", pid);
            kill(pid, SIGKILL);
            return -1;
        }
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
        DFXLOGE("processdump_parser exit with signal(%{public}d)", WTERMSIG(status));
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        DFXLOGI("processdump_parser exit (%{public}d)", WEXITSTATUS(status));
        return 0;
    }
    DFXLOGE("processdump_parser exit with error(%{public}d)", WEXITSTATUS(status));
    return -1;
}

static void CleanFd(int *pipeFd)
{
    if (*pipeFd != -1) {
        syscall(SYS_close, *pipeFd);
        *pipeFd = -1;
    }
}

static void CleanPipe(int *pipeFd)
{
    CleanFd(&pipeFd[0]);
    CleanFd(&pipeFd[1]);
}

static bool InitPipe(int *pipeFd)
{
    if (syscall(SYS_pipe2, pipeFd, 0) == -1) {
        CleanPipe(pipeFd);
        return false;
    }
    return true;
}

using ChildFuncType = std::function<std::string(void)>;

static std::pair<bool, std::string> ExecuteTaskByFork(ChildFuncType func)
{
    auto result = std::make_pair(false, std::string(""));
    int pipefd[2] = {-1, -1};
    if (!InitPipe(pipefd)) {
        DFXLOGE("Create pipe failed. errno:%{public}d", errno);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        DFXLOGE("Fork failed. errno:%{public}d", errno);
        CleanPipe(pipefd);
        return result;
    } else if (pid == 0) {
        DFXLOGI("start in child process.");
        alarm(ALARM_TIME_S);
        prctl(PR_SET_NAME, "processdump_parser");
        CleanFd(&pipefd[0]);
        std::string result = func();
        write(pipefd[1], result.c_str(), result.size());
        CleanFd(&pipefd[1]);
        _exit(0);
    }
    CleanFd(&pipefd[1]);
    std::string formattedStack = "";
    char temp[1024] = {0};
    ssize_t bytesRead;
    while ((bytesRead = OHOS_TEMP_FAILURE_RETRY(read(pipefd[0], temp, sizeof(temp) - 1))) > 0) {
        temp[bytesRead] = '\0';
        formattedStack += std::string(temp, bytesRead);
    }
    CleanFd(&pipefd[0]);
    int res = WaitProcessExitTimeout(pid, WAITPID_TIMEOUT_MS);
    if (res != 0 || formattedStack.empty()) {
        return result;
    }
    result.first = true;
    result.second = formattedStack;
    return result;
}

bool StackJsonFormatter::FormatJsonStack(Json::Value& threads, std::string& outStackStr, std::string& outStackJson)
{
    constexpr int maxThreadCount = 10000;
    if (threads.size() > maxThreadCount) {
        outStackStr.append("Thread count exceeds limit(10000).");
        return false;
    }
    for (uint32_t i = 0; i < threads.size(); ++i) {
        if (!FormatJsonThreadStack(threads[i], outStackStr)) {
            return false;
        }
    }
    outStackJson = Json::FastWriter().write(threads);
    return true;
}

bool StackJsonFormatter::IsStatInfoValid(Json::Value& thread)
{
    const int statInfoCount = 6;
    std::array<const char* const, statInfoCount> statInfo = {
        "state",
        "utime",
        "stime",
        "priority",
        "nice",
        "clk"
    };

    for (const auto& info : statInfo) {
        if (!thread.isMember(info) || !thread[info].isConvertibleTo(Json::stringValue)) {
            return false;
        }
    }
    return true;
}

bool StackJsonFormatter::FormatJsonThreadStack(Json::Value& thread, std::string& outStackStr)
{
    std::string ss;
    // Tid:xxx, Name:test
    if (thread["tid"].isConvertibleTo(Json::stringValue) &&
            thread["thread_name"].isConvertibleTo(Json::stringValue)) {
        ss += "Tid:" + JsonAsString(thread["tid"]) + ", Name:" + JsonAsString(thread["thread_name"]) + "\n";
    }
    // state=R, utime=5, stime=1, priority=-10, nice=-1, clk=100
    if (IsStatInfoValid(thread)) {
        ss += "state=" + JsonAsString(thread["state"]) + ", utime=" + JsonAsString(thread["utime"]) +
            ", stime=" + JsonAsString(thread["stime"]) + ", priority=" + JsonAsString(thread["priority"]) +
            ", nice=" + JsonAsString(thread["nice"]) + ", clk=" + JsonAsString(thread["clk"]) + "\n";
    }
    if (!thread.isMember("frames") || !thread["frames"].isArray()) {
        return true;
    }
    Json::Value& frames = thread["frames"];
    constexpr int maxFrameNum = 1000;
    if (frames.size() > maxFrameNum) {
        return true;
    }
    for (uint32_t j = 0; j < frames.size(); ++j) {
        std::string frameStr = "";
        bool formatStatus = false;
        if (!frames[j].isMember("line")) {
            formatStatus = FormatNativeFrame(frames, j, frameStr);
        } else {
            formatStatus = FormatJsFrame(frames, j, frameStr);
        }
        if (formatStatus) {
            ss += frameStr + "\n";
        } else {
            // Shall we try to print more information?
            outStackStr.append("Frame info is illegal.");
            return false;
        }
    }

    outStackStr.append(ss);
    return true;
}

static bool ParseJsonStack(std::string& jsonStack, int& dumpResult, Json::Value& threads)
{
    Json::Reader reader;
    Json::Value jsonObj;
    if (!(reader.parse(jsonStack, jsonObj))) {
        DFXLOGE("json stack parse failed");
        jsonStack = "";
        return false;
    }

    if (jsonObj.isArray()) {
        dumpResult = 0;
        threads = jsonObj;
        return true;
    }

    if (!(jsonObj.isMember("dump_result") && jsonObj["dump_result"].isInt()) || !jsonObj.isMember("dump_threads")) {
        DFXLOGE("json stack not dump_result or dump_threads");
        jsonStack = "";
        return false;
    }

    dumpResult = jsonObj["dump_result"].asInt();
    threads = jsonObj["dump_threads"];

    if (!threads.isArray()) {
        DFXLOGE("json stack dump_result is not array");
        jsonStack = "";
        return false;
    }

    jsonStack = Json::FastWriter().write(threads);
    return true;
}
} // end namespace

bool DfxJsonFormatter::FormatJsonStack(std::string& jsonStack, std::string& outStackStr,
    bool needParseSymbol, const std::string& bundleName)
{
    int dumpResult = 0;
    Json::Value threads;
    if (!ParseJsonStack(jsonStack, dumpResult, threads)) {
        outStackStr.append("Failed to parse json stack info.");
        jsonStack = "";
        return false;
    }

    auto onlyParseBuildIdTask = [&jsonStack, &threads, &outStackStr, &bundleName]() {
        std::string outStackJson;
        StackJsonFormatter tool(bundleName, true);
        if (tool.FormatJsonStack(threads, outStackStr, outStackJson)) {
            jsonStack = Json::FastWriter().write(threads);
            return true;
        }
        return false;
    };
    if (!(needParseSymbol && (dumpResult > 0))) {
        return onlyParseBuildIdTask();
    }

    std::function<std::string(void)> parseSymbolTask = [&threads, &bundleName]() {
        std::string outStackStr;
        std::string outStackJson;
        StackJsonFormatter tool(bundleName, false);
        if (!tool.FormatJsonStack(threads, outStackStr, outStackJson)) {
            outStackStr = "";
            outStackJson = "";
        }
        return outStackJson + CONNECTION_STACK + outStackStr;
    };

    auto ret = ExecuteTaskByFork(parseSymbolTask);
    if (ret.first) {
        auto pos = ret.second.find(CONNECTION_STACK);
        if (pos != std::string::npos) {
            jsonStack  = ret.second.substr(0, pos);
            outStackStr = ret.second.substr(pos + strlen(CONNECTION_STACK));
            return true;
        }
    }
    return onlyParseBuildIdTask();
}

#ifdef __aarch64__
static const int PC_LEN = 32;
static const int FRAME_NAME_LEN = 256;
static const int FUNC_INFO_LEN = 512;

struct FormatKernelStackParams {
    const std::string& kernelStack;
    std::string& formattedStack;
    bool jsonFormat;
    ParseSymbolOptions options;
    const std::string& fallbackMainThreadStack;
    int32_t pid;
};

static int32_t ExtractPidFromKernelStack(const std::string& kernelStack)
{
    const char* pidMarker = "pid=";
    size_t pos = kernelStack.find(pidMarker);
    if (pos == std::string::npos) {
        return 0;
    }

    size_t numStart = pos + strlen(pidMarker);
    size_t numEnd = numStart;
    while (numEnd < kernelStack.size() && isdigit(static_cast<unsigned char>(kernelStack[numEnd]))) {
        numEnd++;
    }

    if (numEnd == numStart) {
        return 0;
    }

    errno = 0;
    char* endPtr = nullptr;
    long pid = std::strtol(kernelStack.c_str() + numStart, &endPtr, 10); // 10 : decimal
    if (errno == ERANGE || pid < 0 || pid > INT32_MAX || endPtr != kernelStack.c_str() + numEnd) {
        return 0;
    }
    return static_cast<int32_t>(pid);
}

static std::string ExtractMainThreadUserStack(std::string& kernelStack)
{
    const std::string marker = "\nMain thread user stack (unsymbolized):\n";
    size_t pos = kernelStack.find(marker);
    if (pos == std::string::npos) {
        return "";
    }

    std::string userStack = kernelStack.substr(pos + marker.length());
    kernelStack = kernelStack.substr(0, pos);
    return userStack;
}

static size_t FindMainThreadIndex(const std::vector<DfxThreadStack>& processStack, int32_t pid)
{
    for (size_t i = 0; i < processStack.size(); ++i) {
        if (processStack[i].tid == pid) {
            return i;
        }
    }
    return std::string::npos;
}

/**
 * @brief Parse thread info from stack content.
 * Format: "Tid:12926, Name:main\nstate=S, utime=15, stime=17, priority=-14, nice=-10, clk=100"
 */
static bool ParseThreadInfoFromStack(const std::string& stackContent, int32_t pid,
    std::string& threadName, std::string& threadStat)
{
    std::istringstream iss(stackContent);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("Tid:") == std::string::npos) {
            continue;
        }
        std::string tidMarker = "Tid:" + std::to_string(pid);
        if (line.find(tidMarker) == std::string::npos) {
            return false;
        }
        size_t namePos = line.find("Name:");
        if (namePos != std::string::npos) {
            threadName = line.substr(namePos + 5); // 5 : thread name offset
        }
        if (std::getline(iss, line) && line.find("state=") != std::string::npos) {
            threadStat = line;
        }
        return true;
    }
    return false;
}

/**
 * @brief Parse file info from JS stack frame.
 * Format: "/path/to/file.js:123:456" -> fileName="/path/to/file.js", line=123, column=456
 */
static void ParseJsFileInfo(const std::string& fileInfo, std::string& fileName, int32_t& line, int32_t& column)
{
    size_t colonPos2 = fileInfo.rfind(':');
    if (colonPos2 == std::string::npos) {
        fileName = fileInfo;
        return;
    }
    column = std::atoi(fileInfo.substr(colonPos2 + 1).c_str());
    size_t colonPos1 = fileInfo.rfind(':', colonPos2 - 1);
    if (colonPos1 == std::string::npos) {
        fileName = fileInfo;
        return;
    }
    line = std::atoi(fileInfo.substr(colonPos1 + 1, colonPos2 - colonPos1 - 1).c_str());
    fileName = fileInfo.substr(0, colonPos1);
}

/**
 * @brief Parse JS frame string.
 * Format: "#01 at symbol1 packageName (/path/to/test1.js:123:456)" or "#01 at symbol1 (/path/to/test1.js:123:456)"
 * packageName is optional.
 */
static bool ParseJsFrameStr(const std::string& line, DfxFrame& frame)
{
    char funcName[FRAME_NAME_LEN] = {0};
    char packageName[FRAME_NAME_LEN] = {0};
    char fileInfo[FUNC_INFO_LEN] = {0};
    int matched = sscanf_s(line.c_str(), "%*s at %255s %255s (%511[^)])",
        funcName, sizeof(funcName), packageName, sizeof(packageName), fileInfo, sizeof(fileInfo));
    if (matched < 1) {
        return false;
    }

    frame.isJsFrame = true;
    frame.funcName = funcName;
    if (matched >= 2) { // 2 : has packageName or fileInfo
        if (matched == 2 && fileInfo[0] == '\0') {
            matched = sscanf_s(line.c_str(), "%*s at %255s (%511[^)])",
                funcName, sizeof(funcName), fileInfo, sizeof(fileInfo));
            frame.funcName = funcName;
        } else if (matched >= 3) { // 3 : has both packageName and fileInfo
            frame.packageName = packageName;
        }
    }
    if (fileInfo[0] != '\0') {
        ParseJsFileInfo(fileInfo, frame.mapName, frame.line, frame.column);
    }

    return true;
}

/**
 * @brief Parse native frame string.
 * Format examples:
 * - "#00 pc 0000000000173b8c /system/lib/ld-musl-aarch64.so.1"
 * - "#00 pc 0000000000173b8c /system/lib/ld-musl-aarch64.so.1(81437328e14b7fba0bac6c92e21735b9)"
 * - "#00 pc 0000000000173b8c /system/lib/ld-musl-aarch64.so.1(epoll_wait+84)(81437328e14b7fba0bac6c92e21735b9)"
 * - "#03 pc 000000000002fab0 /system/lib64/libeventhandler.z.so(WaitFor(long, bool)+276)(e420922c...)"
 * funcName, funcOffset, and buildId are optional.
 */
static bool ParseNativeFrameStr(const std::string& line, DfxFrame& frame)
{
    char pcStr[PC_LEN] = {0};
    char mapName[FRAME_NAME_LEN] = {0};
    int matched = sscanf_s(line.c_str(), "%*s pc %31s %255[^(]", pcStr, sizeof(pcStr), mapName, sizeof(mapName));
    if (matched < 2) { // 2 : at least pc and mapName
        return false;
    }
    frame.relPc = strtoull(pcStr, nullptr, 16); // 16 : hex
    frame.mapName = mapName;
    size_t firstParen = line.find('(');
    if (firstParen == std::string::npos) {
        return true;
    }
    size_t pos = firstParen;
    int parenCount = 1;
    while (++pos < line.size() && parenCount > 0) {
        parenCount += (line[pos] == '(') ? 1 : (line[pos] == ')' ? -1 : 0);
    }
    if (parenCount != 0) {
        return true;
    }
    std::string firstContent = line.substr(firstParen + 1, pos - firstParen - 2); // 2 : exclude ')'
    size_t plusPos = firstContent.rfind('+');
    if (plusPos == std::string::npos) {
        frame.buildId = firstContent;
        return true;
    }
    frame.funcName = firstContent.substr(0, plusPos);
    frame.funcOffset = strtoull(firstContent.substr(plusPos + 1).c_str(), nullptr, 10); // 10 : decimal
    size_t secondParen = line.find('(', pos);
    if (secondParen != std::string::npos) {
        size_t secondParenEnd = line.find(')', secondParen);
        if (secondParenEnd != std::string::npos) {
            frame.buildId = line.substr(secondParen + 1, secondParenEnd - secondParen - 1);
        }
    }
    return true;
}

/**
 * @brief Parse formatted stack frames from stack string.
 * Supports both JS and native frame formats. Each line starting with '#' is parsed as a frame.
 */
static std::vector<DfxFrame> ParseFormattedFrames(const std::string& formattedStack)
{
    std::vector<DfxFrame> frames;
    std::istringstream iss(formattedStack);
    std::string line;
    size_t index = 0;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] != '#') {
            continue;
        }

        DfxFrame frame;
        frame.index = index++;

        if (!ParseJsFrameStr(line, frame) && !ParseNativeFrameStr(line, frame)) {
            DFXLOGW("Failed to parse frame: %{public}s", line.c_str());
            continue;
        }

        frames.emplace_back(frame);
    }
    return frames;
}

/**
 * @brief main thread stack choose policy function.
 *
 * @param pid  process id
 * @param processStack  input/output process stack
 * @param mainThreadUserStack  input process main thread user stack
 * @param fallbackMainThreadStack  input fallback process main thread user stack
 */
static void ApplyMainThreadStackPriority(const int32_t pid, std::vector<DfxThreadStack>& processStack,
    const std::string& mainThreadUserStack, const std::string& fallbackMainThreadStack)
{
    std::string updateThreadName;
    std::string updateThreadStat;
    size_t mainThreadIdx = FindMainThreadIndex(processStack, pid);
    if (mainThreadIdx == std::string::npos) {
        DFXLOGW("input kernel stack do not have main thread kernel stack.");
        const std::string& stackSource = !mainThreadUserStack.empty() ? mainThreadUserStack : fallbackMainThreadStack;
        if (stackSource.empty()) {
            return;
        }
        if (!ParseThreadInfoFromStack(stackSource, pid, updateThreadName, updateThreadStat)) {
            return;
        }
        DfxThreadStack mainThread;
        mainThread.tid = pid;
        mainThread.threadName = updateThreadName;
        mainThread.threadStat = updateThreadStat;
        mainThread.frames = ParseFormattedFrames(stackSource);
        processStack.push_back(mainThread);
        return;
    }

    if (!mainThreadUserStack.empty() &&
        ParseThreadInfoFromStack(mainThreadUserStack, pid, updateThreadName, updateThreadStat)) {
        processStack[mainThreadIdx].threadName = updateThreadName;
        processStack[mainThreadIdx].threadStat = updateThreadStat;
        processStack[mainThreadIdx].frames = ParseFormattedFrames(mainThreadUserStack);
        DFXLOGI("change to format main thread user stack.");
    } else if (processStack[mainThreadIdx].frames.size() <= 3 && !fallbackMainThreadStack.empty() && // 3 : 3 frames
        ParseThreadInfoFromStack(fallbackMainThreadStack, pid, updateThreadName, updateThreadStat)) {
        processStack[mainThreadIdx].threadName = updateThreadName;
        processStack[mainThreadIdx].threadStat = updateThreadStat;
        processStack[mainThreadIdx].frames = ParseFormattedFrames(fallbackMainThreadStack);
        DFXLOGI("change to format fallback main thread user stack.");
    }
}

static bool FormatKernelStackStr(const std::vector<DfxThreadStack>& processStack, std::string& formattedStack)
{
    if (processStack.empty()) {
        return false;
    }
    formattedStack = "";
    for (const auto &threadStack : processStack) {
        std::string ss = "Tid:" + std::to_string(threadStack.tid) + ", Name:" + threadStack.threadName + "\n";
        formattedStack.append(ss);
        formattedStack.append(threadStack.threadStat).append("\n");
        for (size_t frameIdx = 0; frameIdx < threadStack.frames.size(); ++frameIdx) {
            formattedStack.append(DfxFrameFormatter::GetFrameStr(threadStack.frames[frameIdx]));
        }
    }
    return true;
}

static void FillJsFrameJson(const DfxFrame& frame, Json::Value& frameJson)
{
    frameJson["file"] = frame.mapName;
    frameJson["packageName"] = frame.packageName;
    frameJson["symbol"] = frame.funcName;
    frameJson["line"] = frame.line;
    frameJson["column"] = frame.column;
}

static void FillNativeFrameJson(const DfxFrame& frame, Json::Value& frameJson)
{
    frameJson["pc"] = StringPrintf("%016lx", frame.relPc);
    if (!frame.funcName.empty() && frame.funcName.length() <= MAX_FUNC_NAME_LEN) {
        frameJson["symbol"] = frame.funcName;
    } else {
        frameJson["symbol"] = "";
    }
    frameJson["offset"] = frame.funcOffset;
    frameJson["file"] = frame.mapName.empty() ? "Unknown" : frame.mapName;
    frameJson["buildId"] = frame.buildId;
}

static void FillFrameJson(const DfxFrame& frame, Json::Value& frameJson)
{
    frame.isJsFrame ? FillJsFrameJson(frame, frameJson) : FillNativeFrameJson(frame, frameJson);
}

static bool FormatKernelStackJson(std::vector<DfxThreadStack> processStack, std::string& formattedStack)
{
    if (processStack.empty()) {
        return false;
    }
    Json::Value jsonInfo;
    for (const auto &threadStack : processStack) {
        Json::Value threadInfo;
        threadInfo["thread_name"] = threadStack.threadName;
        threadInfo["tid"] = threadStack.tid;
        char state;
        uint64_t utime;
        uint64_t stime;
        int64_t priority;
        int64_t nice;
        int64_t clk;
        int parsed = sscanf_s(threadStack.threadStat.c_str(),
                "state=%c, utime=%" PRIu64 ", stime=%" PRIu64 ", priority=%" PRIi64 ", nice=%" PRIi64 ", clk=%" PRIi64,
                &state, 1, &utime, &stime, &priority, &nice, &clk);
        constexpr int fieldCnt = 6;
        if (parsed != fieldCnt) {
            DFXLOGE("parser tid %{public}ld field failed, cnt %{public}d", threadStack.tid, parsed);
        } else {
            threadInfo["state"] = std::string(1, state);
            threadInfo["utime"] = utime;
            threadInfo["stime"] = stime;
            threadInfo["priority"] = priority;
            threadInfo["nice"] = nice;
            threadInfo["clk"] = clk;
        }

        Json::Value frames(Json::arrayValue);
        for (const auto& frame : threadStack.frames) {
            Json::Value frameJson;
            FillFrameJson(frame, frameJson);
            frames.append(frameJson);
        }
        threadInfo["frames"] = frames;
        jsonInfo.append(threadInfo);
    }
    formattedStack = Json::FastWriter().write(jsonInfo);
    return true;
}

void ReportDumpStats(const std::string& bundleName, ParseTime& parseTime, uint32_t threadCount)
{
    auto costTime = parseTime.formatKernelStackTime +
        parseTime.parseBuildIdJsSymbolTime + parseTime.parseNativeSymbolTime;
    if (costTime <= MAX_ALL_FRAME_PARSE_TIME_MS) {
        return;
    }
    ReportData reportData;
    reportData.parseCostType = ParseCostType::PARSE_ALL_FRAME_TIME;
    reportData.bundleName = bundleName;
    reportData.parseTime = parseTime;
    reportData.costTime = costTime;
    reportData.threadCount = threadCount;
    DfxOfflineParser::ReportDumpStats(reportData);
}

void ParseSymbolWithDfxThreadStack(std::vector<DfxThreadStack>& processStack, const std::string& bundleName,
    uint32_t timeout, ParseTime& parseTime)
{
    uint32_t beginTime = static_cast<uint32_t>(GetAbsTimeMilliSeconds());
    std::shared_ptr<DfxOfflineParser> parser = std::make_shared<DfxOfflineParser>(bundleName);
    if (!parser) {
        DFXLOGE("parser is nullptr!");
        return;
    }
    for (auto& threadStack : processStack) {
        parser->ParseBuildIdJsSymbolWithFrames(threadStack.frames);
    }
    uint32_t parseBuildIdJsSymbolTime = static_cast<uint32_t>(GetAbsTimeMilliSeconds());
    parseTime.parseBuildIdJsSymbolTime = parseBuildIdJsSymbolTime - beginTime;
    if (timeout <= parseTime.parseBuildIdJsSymbolTime) {
        DFXLOGE("parseBuildIdJsSymbolTime is over timeout!");
        return;
    }
    timeout -= parseTime.parseBuildIdJsSymbolTime;
    if (timeout > PARSE_SO_SYMBOL_REMAIN_TIME_MS + LESS_REMAIN_TIME_MS) {
        auto timeRemain = static_cast<time_t>(std::chrono::milliseconds(PARSE_SO_SYMBOL_REMAIN_TIME_MS).count());
        ElapsedTime et;
        for (auto& threadStack : processStack) {
            parser->ParseNativeSymbolWithFrames(threadStack.frames);
            auto timeSpend = et.Elapsed<std::chrono::milliseconds>();
            if (timeRemain <= timeSpend) {
                DFXLOGE("parse so symbol timeout");
                break;
            }
        }
    } else {
        DFXLOGE("not enough time to parse so symbol!");
    }
    uint32_t parseNativeSymbolTime = static_cast<uint32_t>(GetAbsTimeMilliSeconds());
    parseTime.parseNativeSymbolTime = parseNativeSymbolTime - parseBuildIdJsSymbolTime;
    DFXLOGI("ParseSymbolWithDfxThreadStack parseBuildIdJsSymbolTime=%{public}u ms, "
            "parseNativeSymbolTime=%{public}u ms", parseTime.parseBuildIdJsSymbolTime, parseTime.parseNativeSymbolTime);
    ReportDumpStats(bundleName, parseTime, static_cast<uint32_t>(processStack.size()));
}

bool FormatKernelStackImpl(const FormatKernelStackParams& params)
{
    uint32_t timeout = static_cast<uint32_t>(ALARM_TIME_S * 1000);
    uint32_t beginTime = static_cast<uint32_t>(GetAbsTimeMilliSeconds());

    std::string pureKernelStack = params.kernelStack;
    std::string mainThreadUserStack = ExtractMainThreadUserStack(pureKernelStack);

    std::vector<DfxThreadStack> processStack;
    if (!FormatProcessKernelStack(pureKernelStack, processStack)) {
        return false;
    }

    ApplyMainThreadStackPriority(params.pid, processStack, mainThreadUserStack, params.fallbackMainThreadStack);

    ParseTime parseTime;
    parseTime.formatKernelStackTime = static_cast<uint32_t>(GetAbsTimeMilliSeconds()) - beginTime;
    timeout = (timeout > parseTime.formatKernelStackTime) ? timeout - parseTime.formatKernelStackTime : 0;
    if (params.options.needParseSymbol && timeout > 0) {
        DfxEnableTraceDlsym(true);
        ParseSymbolWithDfxThreadStack(processStack, params.options.bundleName, timeout, parseTime);
        DfxEnableTraceDlsym(false);
    } else {
        DFXLOGI("no need to parse symbol or timeout is not enough!");
    }
    if (params.jsonFormat) {
        return FormatKernelStackJson(processStack, params.formattedStack);
    } else {
        return FormatKernelStackStr(processStack, params.formattedStack);
    }
}

#endif

bool DfxJsonFormatter::FormatKernelStack(const std::string& kernelStack, std::string& formattedStack, bool jsonFormat,
    bool needParseSymbol, const std::string& bundleName, const std::string& fallbackMainThreadStack)
{
#ifdef __aarch64__
    ParseSymbolOptions options(needParseSymbol, bundleName);
    int32_t pid = ExtractPidFromKernelStack(kernelStack);

    FormatKernelStackParams params {
        .kernelStack = kernelStack,
        .formattedStack = formattedStack,
        .jsonFormat = jsonFormat,
        .options = options,
        .fallbackMainThreadStack = fallbackMainThreadStack,
        .pid = pid
    };

    if (!needParseSymbol) {
        return FormatKernelStackImpl(params);
    }

    std::function<std::string(void)> func = [params]() mutable {
            std::string formattedStack;
            FormatKernelStackParams localParams = params;
            localParams.formattedStack = formattedStack;
            (void)FormatKernelStackImpl(localParams);
            return localParams.formattedStack;
        };

    auto ret = ExecuteTaskByFork(func);
    if (ret.first) {
        formattedStack = ret.second;
        return true;
    }
    options.needParseSymbol = false;
    FormatKernelStackParams noSymbolParams = params;
    noSymbolParams.options = options;
    return FormatKernelStackImpl(noSymbolParams);
#else
    return false;
#endif
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
