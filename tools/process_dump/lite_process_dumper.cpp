/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "lite_process_dumper.h"

#include <map>
#include <memory>
#include <vector>

#ifndef is_ohos_lite
#include "parameters.h"
#endif

#include "decorative_dump_info.h"
#include "dump_info_json_formatter.h"
#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_dump_request.h"
#include "dfx_dump_res.h"
#include "dfx_exception.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "dump_info_json_formatter.h"
#include "dump_utils.h"
#ifndef is_ohos_lite
#include "faultlog_client.h"
#endif
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#include "hisysevent_c.h"
#endif
#include "procinfo.h"
#include "reporter.h"
#include <sys/prctl.h>
#include <sys/time.h>
#include "thread_context.h"
#include "unwinder.h"
#include "unwinder_config.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "LiteProcessDumper"

namespace OHOS {
namespace HiviewDFX {

bool LiteProcessDumper::ReadRequest(int pipeReadFd)
{
    if (!LoopReadPipe(pipeReadFd, &request_, sizeof(struct ProcessDumpRequest))) {
        DFXLOGI("failed to read request %{public}d", errno);
        return false;
    }
    isJsonDump_ =  request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH &&
        request_.siginfo.si_code == DUMP_TYPE_REMOTE_JSON;
    DFXLOGI("read remote request procname: %{public}s", request_.processName);
    return true;
}

void LiteProcessDumper::SetProcessdumpTimeout(siginfo_t &si)
{
    if (si.si_signo != SIGDUMP) {
        return;
    }

    int tid;
    ParseSiValue(si, expectedDumpFinishTime_, tid);

    if (tid == 0) {
        DFXLOGI("reset, prevent incorrect reading sival_int for tid");
        si.si_value.sival_int = 0;
    }

    if (expectedDumpFinishTime_ == 0) {
        DFXLOGI("end time is zero, not set new alarm");
        return;
    }

    uint64_t curTime = GetAbsTimeMilliSeconds();
    if (curTime >= expectedDumpFinishTime_) {
        DFXLOGI("now has timeout, processdump exit");
#ifndef CLANG_COVERAGE
        _exit(0);
#endif
    }
    uint64_t diffTime = expectedDumpFinishTime_ - curTime;

    DFXLOGI("processdump remain time%{public}" PRIu64 "ms", diffTime);
    if (diffTime > PROCESSDUMP_TIMEOUT * NUMBER_ONE_THOUSAND) {
        DFXLOGE("dump remain time is invalid, not set timer");
        return;
    }

    struct itimerval timer{};
    timer.it_value.tv_sec = static_cast<int64_t>(diffTime / NUMBER_ONE_THOUSAND);
    timer.it_value.tv_usec = static_cast<int64_t>(diffTime * NUMBER_ONE_THOUSAND % NUMBER_ONE_MILLION);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        DFXLOGE("start processdump timer fail %{public}d", errno);
    }
}

bool LiteProcessDumper::ReadStat(int pipeReadFd)
{
    char buf[PROC_STAT_BUF_SIZE];
    if (!LoopReadPipe(pipeReadFd, buf, sizeof(buf))) {
        DFXLOGI("failed to read stat %{public}d", errno);
        return false;
    }
    buf[PROC_STAT_BUF_SIZE - 1] = '\0';
    stat_ = buf;
    DFXLOGI("stat %{public}s", stat_.c_str());
    return true;
}

bool LiteProcessDumper::ReadStatm(int pipeReadFd)
{
    char buf[PROC_STATM_BUF_SIZE];
    if (!LoopReadPipe(pipeReadFd, buf, sizeof(buf))) {
        DFXLOGI("failed to read statm %{public}d", errno);
        return false;
    }
    buf[PROC_STATM_BUF_SIZE - 1] = '\0';
    statm_ = buf;
    DFXLOGI("statm %{public}s", statm_.c_str());
    return true;
}

bool LiteProcessDumper::ReadStack(int pipeReadFd)
{
    stackBuf_.resize(PRIV_COPY_STACK_BUFFER_SIZE);
    if (!LoopReadPipe(pipeReadFd, stackBuf_.data(), PRIV_COPY_STACK_BUFFER_SIZE)) {
        DFXLOGE("read stack fail %{public}d", errno);
        return false;
    }
    DFXLOGI("read stack success");
    return ReadOtherThreadStack(pipeReadFd);
}

bool LiteProcessDumper::ReadOtherThreadStack(int pipeReadFd)
{
    int otherThreadNum = 0;
    if (!LoopReadPipe(pipeReadFd, &otherThreadNum, sizeof(int))) {
        DFXLOGE("read otherThreadNum fail %{public}d", errno);
        return false;
    }
    if (otherThreadNum < 0 || otherThreadNum > MAX_DUMP_THREAD_NUM) {
        DFXLOGE("lite proc dump otherThreadNum %{public}d invalid", otherThreadNum);
        return false;
    }

    otherThreadRequest_.resize(otherThreadNum);
    otherThreadStackBuf_.resize(otherThreadNum);
    for (size_t i = 0; i < otherThreadRequest_.size() && i < otherThreadStackBuf_.size(); ++i) {
        if (!LoopReadPipe(pipeReadFd, &otherThreadRequest_[i], sizeof(ThreadDumpRequest))) {
            DFXLOGE("read thread request fail %{public}d", errno);
            return false;
        }

        otherThreadStackBuf_[i].resize(THREAD_STACK_BUFFER_SIZE);
        if (!LoopReadPipe(pipeReadFd, otherThreadStackBuf_[i].data(), THREAD_STACK_BUFFER_SIZE)) {
            DFXLOGE("read thread stack fail %{public}d", errno);
            return false;
        }
    }
    DFXLOGI("read %{public}d other thread stack success", otherThreadNum);
    return true;
}

bool LiteProcessDumper::LoopReadPipe(int pipeReadFd, void* buf, size_t length)
{
    const size_t step = 1024 * 1024;
    size_t readSuccessSize = 0;
    size_t totalReadSize = 0;
    const size_t maxTryTimes = 1000;
    for (size_t i = 0; i < length; i += readSuccessSize) {
        size_t len = (i + step) < length ? step : length - i;
        int savedErrno = 0;
        ssize_t readSize = 0;
        size_t tryTimes = 0;
        do {
            readSize = read(pipeReadFd, static_cast<char*>(buf) + readSuccessSize, len);
            savedErrno = errno;
            if (readSize == -1 && savedErrno != EINTR && savedErrno != EAGAIN) {
                return false;
            }
            if (readSize > 0) {
                readSuccessSize = readSize;
                totalReadSize += readSize;
            }
            if (tryTimes > maxTryTimes && readSize == -1) {
                DFXLOGW("LoopReadPipe exceeding the maximum number of retries!");
                return totalReadSize == length;
            }
            if (readSize == -1 && savedErrno == EAGAIN) {
                ++tryTimes;
                usleep(1000); // 1000 : sleep 1ms try agin
            }
        } while (readSize == -1 && (savedErrno == EINTR || savedErrno == EAGAIN));
    }
    return totalReadSize == length;
}

MemoryBlockInfo LiteProcessDumper::ReadSingleRegMem(int pipeReadFd, uintptr_t nameAddr,
    unsigned int count, unsigned int forward)
{
    std::vector<uintptr_t> bk(count);
    if (!LoopReadPipe(pipeReadFd, bk.data(), bk.size() * sizeof(uintptr_t))) {
        DFXLOGI("failed to read reg near memory %{public}d", errno);
    }
    MemoryBlockInfo info;
    info.nameAddr = nameAddr;
    info.startAddr = info.nameAddr  - forward * sizeof(uintptr_t);
    info.size =  bk.size();
    info.content = bk;
    return info;
}

bool LiteProcessDumper::ReadMemoryNearRegister(int pipeReadFd, ProcessDumpRequest request)
{
    constexpr int bufSize = 50;
    char data[bufSize];
    char startTag[bufSize] = "start trans register";
    if (!LoopReadPipe(pipeReadFd, data, bufSize) || std::strcmp(data, startTag) != 0) {
        DFXLOGE("failed to start memory tag %{public}d", errno);
        return false;
    }
#if defined(__aarch64__)
    auto& memoryIns = MemoryNearRegisterUtil::GetInstance();
    DFXLOGI("start memory tag %{public}s", data);
    // read reg x0 - x29 near memory
    for (int i = 0; i < 30; i++) { // 30 : lr
        auto info = ReadSingleRegMem(pipeReadFd, request.context.uc_mcontext.regs[i],
            COMMON_REG_MEM_SIZE, COMMON_REG_MEM_FORWARD_SIZE);
        memoryIns.blocksInfo_.push_back(info);
    }
    memoryIns.blocksInfo_.push_back(ReadSingleRegMem(pipeReadFd, request.context.uc_mcontext.regs[30],  // 30 : lr
        SPECIAL_REG_MEM_SIZE, SPECIAL_REG_MEM_FORWARD_SIZE));
    memoryIns.blocksInfo_.push_back(ReadSingleRegMem(pipeReadFd, request.context.uc_mcontext.sp,
        COMMON_REG_MEM_SIZE, COMMON_REG_MEM_FORWARD_SIZE));
    memoryIns.blocksInfo_.push_back(ReadSingleRegMem(pipeReadFd, request.context.uc_mcontext.pc,
        SPECIAL_REG_MEM_SIZE, SPECIAL_REG_MEM_FORWARD_SIZE));
#endif
    char endTag[bufSize] = "end trans register";
    if (!LoopReadPipe(pipeReadFd, data, bufSize) || std::strcmp(data, endTag) != 0) {
        DFXLOGE("failed to end memory tag %{public}d", errno);
        return false;
    }
    DFXLOGI("end memory tag %{public}s", data);
    return true;
}

bool LiteProcessDumper::ReadContent(int pipeReadFd)
{
    if (!ReadRequest(pipeReadFd)) {
        return false;
    }
    SetProcessdumpTimeout(request_.siginfo);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        if (!ReadStat(pipeReadFd) || !ReadStatm(pipeReadFd)) {
            return false;
        }
    }
    if (!ReadStack(pipeReadFd)) {
        return false;
    }
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        if (!ReadMemoryNearRegister(pipeReadFd, request_)) {
            return false;
        }
    }
    std::vector<char> data(MAX_PIPE_SIZE, 0);
    ssize_t n = 1;
    size_t tryTimes = 0;
    constexpr size_t maxTryTimes = 200;
    while ((n = read(pipeReadFd, data.data(), MAX_PIPE_SIZE)) > 0 ||
        (n == -1 && (errno == EAGAIN || errno == EINTR))) {
        if (n == -1 && errno == EAGAIN) {
            if (++tryTimes > maxTryTimes) {
                DFXLOGW("read pipe data time out");
                break;
            }
            usleep(1000); // 1000 : sleep 1ms try agin
        }
        if (n > 0) {
            rawData_.append(data.data(), static_cast<size_t>(n));
        }
    }
    return true;
}

bool LiteProcessDumper::ReadPipeData(int pid)
{
    DFXLOGI("start read pipe data");
    int pipeReadFd = -1;
#ifndef is_ohos_lite
    RequestLimitedPipeFd(PIPE_READ, &pipeReadFd, pid, nullptr);
#endif
    if (pipeReadFd <= 0) {
        return false;
    }
    if (!ReadContent(pipeReadFd)) {
#ifndef is_ohos_lite
        RequestLimitedDelPipeFd(pid);
#endif
        return false;
    }
    DFXLOGI("finish read pipe data");
#ifndef is_ohos_lite
    RequestLimitedDelPipeFd(pid);
#endif
    return true;
}

void LiteProcessDumper::Unwind()
{
    if (dfxMaps_ == nullptr || stackBuf_.empty()) {
        DFXLOGE("dfxMaps or stackBuf is empyt");
        return;
    }
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.SetMaps(dfxMaps_);
    instance.SetStackForward(PRIV_STACK_FORWARD_BUF_SIZE);
    instance.SetStackBuf(stackBuf_);
    instance.CopyRegister(&request_.context);

    unwinder_ = std::make_shared<Unwinder>(instance.CreateAccessors(), true);
    unwinder_->EnableFillFrames(true);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
#if defined(PROCESSDUMP_MINIDEBUGINFO)
        UnwinderConfig::SetEnableMiniDebugInfo(true);
        UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    }
    unwinder_->SetRegs(DfxRegs::CreateFromUcontext(request_.context));
    unwinder_->SetMaps(dfxMaps_);
    unwinder_->Unwind(&request_.context);
    UnwindOtherThread();
}

void LiteProcessDumper::UnwindOtherThread()
{
    if (otherThreadStackBuf_.size() == 0 || otherThreadRequest_.size() == 0) {
        DFXLOGE("no other thread stack");
        return;
    }
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.SetStackForward(0);
    for (size_t i = 0; i < otherThreadRequest_.size() && i < otherThreadStackBuf_.size(); ++i) {
        instance.SetStackBuf(otherThreadStackBuf_[i]);
        instance.CopyRegister(&otherThreadRequest_[i].context);

        auto unwinder = std::make_shared<Unwinder>(instance.CreateAccessors(), true);
        unwinder->EnableFillFrames(true);
        unwinder->SetRegs(DfxRegs::CreateFromUcontext(otherThreadRequest_[i].context));
        unwinder->SetMaps(dfxMaps_);
        unwinder->Unwind(&otherThreadRequest_[i].context);
        auto &thread = process_->GetOtherThreads();
        thread[i]->SetThreadName(otherThreadRequest_[i].threadName);
        thread[i]->SetFrames(unwinder->GetFrames());
    }
}

void LiteProcessDumper::InitProcess()
{
    process_ = std::make_shared<DfxProcess>();
    process_->InitProcessInfo(request_.pid, request_.nsPid, request_.uid, request_.processName);
    process_->SetLogSource("liteprocessdump");
    process_->SetFaultThreadRegisters(regs_);
    process_->InitKeyThread(request_, false);
    for (size_t i = 0; i < otherThreadRequest_.size(); ++i) {
        auto thread = DfxThread::Create(request_.pid, otherThreadRequest_[i].tid, otherThreadRequest_[i].nsTid,
            request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH);
        process_->GetOtherThreads().push_back(thread);
    }
}

bool LiteProcessDumper::Dump(int pid)
{
    if (!ReadPipeData(pid)) {
        return false;
    }
    if (!DfxBufferWriter::GetInstance().InitBufferWriter(request_)) {
        DFXLOGE("Failed to init buffer writer.");
    }
    regs_ = DfxRegs::CreateFromUcontext(request_.context);
    std::string bundleName = request_.processName;
    bundleName = bundleName.substr(0, bundleName.find_first_of(':'));
    dfxMaps_ = DfxMaps::CreateByBuffer(bundleName, rawData_);
    CollectOpenFiles();
    MmapJitSymbol();
    InitProcess();
    Unwind();
    PrintAll();
    FormatJsonInfoIfNeed();
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        DfxBufferWriter::GetInstance().WriteDumpRes(DumpErrorCode::DUMP_ESUCCESS);
    }
    MunmapJitSymbol();
    Report();
    DFXLOGI("dump finish.");
    return true;
}

void LiteProcessDumper::FormatJsonInfoIfNeed()
{
    if (!isJsonDump_ && request_.type != ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        return;
    }
    if (process_ == nullptr) {
        return;
    }
    std::string jsonInfo;
    DumpInfoJsonFormatter::GetJsonFormatInfo(request_, *process_, jsonInfo);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        process_->SetCrashInfoJson(jsonInfo);
    } else if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        DfxBufferWriter::GetInstance().WriteMsg(jsonInfo);
    }
    DFXLOGI("Finish GetJsonFormatInfo len %{public}" PRIuPTR "", jsonInfo.length());
}

void LiteProcessDumper::PrintHeader()
{
    std::string headInfo;
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        headInfo += StringPrintf("Build info:%s\n", DumpUtils::GetBuildInfo().c_str());
    }
    headInfo += "Timestamp:" + GetCurrentTimeStr(request_.timeStamp);
    headInfo += StringPrintf("Pid:%d\n", request_.pid);
    headInfo += StringPrintf("Uid:%d\n", request_.uid);
    headInfo += StringPrintf("Process name:%s\n", request_.processName);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        uint64_t lifeTimeSeconds;
        GetProcessLifeCycle(stat_, lifeTimeSeconds);
        process_->SetLifeTime(lifeTimeSeconds);
        headInfo += "Process life time:" + std::to_string(lifeTimeSeconds) + "s" + "\n";

        uint64_t rss = GetProcRssMemInfo(statm_);
        process_->SetRss(rss);
        headInfo += StringPrintf("Process Memory(kB): %" PRIu64 "(Rss)\n", rss);

        if (request_.siginfo.si_pid == request_.pid) {
            request_.siginfo.si_uid = request_.uid;
        }
        std::string reason = DfxSignal::PrintSignal(request_.siginfo) + "\n";
        process_->SetReason(DfxSignal::PrintSignal(request_.siginfo));
        headInfo += "Reason:" + reason;
    }
    if (!isJsonDump_) {
        DfxBufferWriter::GetInstance().WriteMsg(headInfo);
    }
    DFXLOGI("%{public}s", headInfo.c_str());
}

void LiteProcessDumper::PrintThreadInfo()
{
    if (unwinder_ == nullptr) {
        return;
    }
    std::string faultThreadInfo;
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        faultThreadInfo += "Fault thread info:\n";
    }
    auto keyThread = process_->GetKeyThread();
    if (keyThread == nullptr) {
        return;
    }
    keyThread->SetFrames(unwinder_->GetFrames());
    faultThreadInfo += keyThread->ToString();
    if (!isJsonDump_) {
        auto& instance = DfxBufferWriter::GetInstance();
        instance.WriteMsg(faultThreadInfo);
    }
    std::istringstream iss(faultThreadInfo);
    std::string line;
    while (std::getline(iss, line)) {
        DFXLOGI("%{public}s", line.c_str());
    }
}

void LiteProcessDumper::PrintOtherThreadInfo()
{
    std::string otherThreadInfo;
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        otherThreadInfo += "Other thread info:\n";
    }
    for (const auto& thread : process_->GetOtherThreads()) {
        otherThreadInfo += thread->ToString();
    }
    if (!isJsonDump_) {
        auto& instance = DfxBufferWriter::GetInstance();
        instance.WriteMsg(otherThreadInfo);
    }
}

void LiteProcessDumper::PrintRegisters()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        return;
    }
    if (regs_ == nullptr) {
        return;
    }
    auto& instance = DfxBufferWriter::GetInstance();
    std::string regsStr = regs_->PrintRegs();
    instance.WriteMsg(regsStr);
    std::istringstream iss(regsStr);
    std::string line;
    while (std::getline(iss, line)) {
        DFXLOGI("%{public}s", line.c_str());
    }
}

void LiteProcessDumper::PrintRegsNearMemory()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        return;
    }
    if (unwinder_ == nullptr) {
        return;
    }
    MemoryNearRegister nearRegMemory;
    nearRegMemory.SetLiteType(true);
    nearRegMemory.Collect(*process_, request_, *unwinder_);
    nearRegMemory.Print(*process_, request_, *unwinder_);
}

void LiteProcessDumper::PrintFaultStack()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        return;
    }
    if (unwinder_ == nullptr) {
        return;
    }
    FaultStack faultStack;
    faultStack.SetLiteType(true);
    faultStack.Collect(*process_, request_, *unwinder_);
    faultStack.Print(*process_, request_, *unwinder_);
}

void LiteProcessDumper::PrintMaps()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        return;
    }
    if (dfxMaps_ == nullptr) {
        return;
    }
    std::string mapsStr = "Maps:\n";
    for (const auto &map : dfxMaps_->GetMaps()) {
        mapsStr.append(map->ToString());
    }
    DfxBufferWriter::GetInstance().WriteMsg(mapsStr);
}

void LiteProcessDumper::CollectOpenFiles()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        return;
    }
    std::istringstream iss(rawData_);
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "end trans openfiles") {
            DFXLOGI("find open file start, end prase_maps");
            std::string rest;
            rest.assign(std::istreambuf_iterator<char>(iss),
                        std::istreambuf_iterator<char>());
            rawData_ = rest;
            break;
        }
        auto pos =  line.find_first_of('-');
        if (pos != std::string::npos) {
            long fd;
            if (!SafeStrtol(line.substr(0, pos).c_str(), &fd, DECIMAL_BASE)) {
                continue;
            }
            line += "\n";
            fdFiles_.emplace(fd, line);
        }
    }
}

void LiteProcessDumper::PrintOpenFiles()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        return;
    }
    std::string openFiles = "OpenFiles:\n";
    DfxBufferWriter::GetInstance().WriteMsg(openFiles);
    for (auto& file : fdFiles_) {
        DfxBufferWriter::GetInstance().WriteMsg(file.second);
    }
}

void LiteProcessDumper::MmapJitSymbol()
{
    jitSymbolMMap_ = mmap(NULL, ARKWEB_JIT_SYMBOL_BUF_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (jitSymbolMMap_ == MAP_FAILED) {
        DFXLOGW("Failed to mmap!\n");
        return;
    }
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, jitSymbolMMap_, ARKWEB_JIT_SYMBOL_BUF_SIZE, "JS_JIT_symbol");
    if (memcpy_s(jitSymbolMMap_, ARKWEB_JIT_SYMBOL_BUF_SIZE, rawData_.c_str(), rawData_.length()) != EOK) {
        munmap(jitSymbolMMap_, ARKWEB_JIT_SYMBOL_BUF_SIZE);
        jitSymbolMMap_ = MAP_FAILED;
        DFXLOGE("Failed to copy rawData_.");
    }
}

void LiteProcessDumper::MunmapJitSymbol()
{
    if (jitSymbolMMap_ == MAP_FAILED) {
        return;
    }
    if (munmap(jitSymbolMMap_, ARKWEB_JIT_SYMBOL_BUF_SIZE) == -1) {
        DFXLOGE("munmap jit symbol failed %{public}d", errno);
    }
    jitSymbolMMap_ = MAP_FAILED;
}

void LiteProcessDumper::PrintAll()
{
    PrintHeader();
    PrintThreadInfo();
    PrintRegisters();
    PrintOtherThreadInfo();
    PrintRegsNearMemory();
    PrintFaultStack();
    PrintMaps();
    PrintOpenFiles();
    DFXLOGI("finish print all to file");
}

bool LiteProcessDumper::Report()
{
    SysEventReporter reporter(request_.type);
    reporter.Report(*process_, request_);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
