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
#include <securec.h>
#include "dfx_kernel_stack.h"
#ifndef is_ohos_lite
#include "dfx_frame_formatter.h"
#include "dfx_offline_parser.h"
#include "json/json.h"
#endif
#include "dfx_log.h"
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
MAYBE_UNUSED const int ALARM_TIME = 10;
MAYBE_UNUSED const int WAITPID_TIMEOUT = 3000;
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
    if (frames[frameIdx].isMember("offset") && frames[frameIdx]["offset"].isInt()) {
        frame.funcOffset = frames[frameIdx]["offset"].asInt();
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
        alarm(ALARM_TIME);
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
    int res = WaitProcessExitTimeout(pid, WAITPID_TIMEOUT);
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

bool StackJsonFormatter::FormatJsonThreadStack(Json::Value& thread, std::string& outStackStr)
{
    std::string ss;
    // Tid:xxx, Name:test
    if (thread["tid"].isConvertibleTo(Json::stringValue) &&
            thread["thread_name"].isConvertibleTo(Json::stringValue)) {
        ss += "Tid:" + JsonAsString(thread["tid"]) + ", Name:" + JsonAsString(thread["thread_name"]) + "\n";
    }
    // state=R, utime=5, stime=1, priority=-10, nice=-1, clk=100
    if (thread["state"].isConvertibleTo(Json::stringValue) &&
        thread["utime"].isConvertibleTo(Json::stringValue) &&
        thread["stime"].isConvertibleTo(Json::stringValue) &&
        thread["priority"].isConvertibleTo(Json::stringValue) &&
        thread["nice"].isConvertibleTo(Json::stringValue) &&
        thread["clk"].isConvertibleTo(Json::stringValue)) {
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
            continue;
        }
        threadInfo["state"] = std::string(1, state);
        threadInfo["utime"] = utime;
        threadInfo["stime"] = stime;
        threadInfo["priority"] = priority;
        threadInfo["nice"] = nice;
        threadInfo["clk"] = clk;
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

bool FormatKernelStackImpl(const std::string& kernelStack, std::string& formattedStack, bool jsonFormat,
    const ParseSymbolOptions& options)
{
    std::vector<DfxThreadStack> processStack;
    if (!FormatProcessKernelStack(kernelStack, processStack, options.needParseSymbol, options.bundleName,
        options.onlyParseBuildId)) {
        return false;
    }
    if (jsonFormat) {
        return FormatKernelStackJson(processStack, formattedStack);
    } else {
        return FormatKernelStackStr(processStack, formattedStack);
    }
}

#endif

bool DfxJsonFormatter::FormatKernelStack(const std::string& kernelStack, std::string& formattedStack, bool jsonFormat,
    bool needParseSymbol, const std::string& bundleName)
{
#ifdef __aarch64__
    ParseSymbolOptions options(needParseSymbol, bundleName);
    if (!needParseSymbol) {
        return FormatKernelStackImpl(kernelStack, formattedStack, jsonFormat, options);
    }

    std::function<std::string(void)> func = [&kernelStack, &jsonFormat, options]() mutable {
            std::string formattedStack;
            options.onlyParseBuildId = false;
            (void)FormatKernelStackImpl(kernelStack, formattedStack, jsonFormat, options);
            return formattedStack;
        };

    auto ret = ExecuteTaskByFork(func);
    if (ret.first) {
        formattedStack = ret.second;
        return true;
    }
    return FormatKernelStackImpl(kernelStack, formattedStack, jsonFormat, options);
#else
    return false;
#endif
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
