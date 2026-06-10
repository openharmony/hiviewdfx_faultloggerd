/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#ifndef is_ohos_lite
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

#include "bundle_mgr_client.h"
#include "bundle_mgr_proxy.h"
#include "cppcrash_info_collector.h"
#include "dfx_buffer_writer.h"
#include "dfx_log.h"
#include "dfx_regs.h"
#include "dfx_signal.h"
#include "dfx_socket_request.h"
#include "dfx_util.h"
#include "dump_utils.h"
#include "faultloggerd_client.h"
#include "lite_process_dumper.h"
#include "minidump_dumper.h"
#include "procinfo.h"
#include "reporter.h"
#include "thread_context.h"
#include "unwinder.h"
#include "unwinder_config.h"
namespace OHOS {
namespace HiviewDFX {
constexpr size_t BUFFER_SIZE = 4096;

bool MinidumpDumper::Dump(int pid, int pipeFd, bool enableMinidump, bool enableMinidumpToCrashLog)
{
    CollectDumpHeaderInfo(pid);
    if (!GenerateMinidump(pid, pipeFd, enableMinidump)) {
        DFXLOGE("Failed to generate minidump file!");
        return false;
    }
    if (enableMinidumpToCrashLog) {
        if (!DfxBufferWriter::GetInstance().InitBufferWriter(request_)) {
            DFXLOGE("Failed to init buffer writer.");
        }
        if (!ParseMinidump()) {
            DFXLOGE("Failed to parse minidump!");
            return false;
        }
        UnwindProcess();
        PrintDumpInfo();
        DfxBufferWriter::GetInstance().WriteFormatCrashInfo();
    }
    return true;
}

void MinidumpDumper::CollectDumpHeaderInfo(int pid)
{
    request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    request_.timeStamp = GetTimeMilliSeconds();
    request_.pid = pid;
    request_.nsPid = pid;
    long uid = 0;
    uint64_t sigBlk = 0;
    GetUidAndSigBlk(pid, uid, sigBlk);
    request_.uid = uid;
    std::string processName;
    ReadProcessName(pid, processName);
    if (strcpy_s(request_.processName, sizeof(request_.processName), processName.c_str()) != EOK) {
        DFXLOGE("strcpy process name failed errno %{public}d", errno);
    }
    uint64_t lifeTimeSeconds = 0;
    GetProcessLifeCycle(pid, lifeTimeSeconds);
    uint64_t rss = GetProcRssMemInfo(pid);
    process_.InitProcessInfo(request_.pid, request_.nsPid, request_.uid, request_.processName);
    process_.SetLogSource("pdump");
    process_.SetLifeTime(lifeTimeSeconds);
    process_.SetRss(rss);
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
}

void MinidumpDumper::ConfigurePerformance(MinidumpParser &minidumpParser)
{
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = true;
    config.bitmapGranularity = 134217728; // 134217728 : Divide the address space into 128MB granularity

    minidumpParser.SetPerformanceConfig(config);
    DFXLOGI("Performance optimization enabled");
}

bool MinidumpDumper::ParseMinidump()
{
    auto startTime = std::chrono::steady_clock::now();
    lseek(outputFdGuard_.GetFd(), 0, SEEK_SET);
    std::string fdPath = "/proc/self/fd/" + std::to_string(outputFdGuard_.GetFd());
    auto input = std::make_shared<std::ifstream>(fdPath, std::ios::binary);
    MinidumpParser minidumpParser(input);

    ConfigurePerformance(minidumpParser);

    if (!minidumpParser.Parse()) {
        DFXLOGE("Failed to read minidumpParser file");
        DFXLOGE("Note: This version only supports little-endian minidumpParser files");
        const auto& error = minidumpParser.GetLastError();
        DFXLOGE("Last error: %{public}s", error.ToString().c_str());
        return false;
    }
    
    if (!ParseExceptionStream(minidumpParser)) {
        DFXLOGE("Failed to parse exception stream");
    }
    if (!ParseThreadNameStream(minidumpParser)) {
        DFXLOGE("Failed to parse thread name stream");
    }
    if (!ParseThreadListStream(minidumpParser)) {
        DFXLOGE("Failed to parse thread list stream");
    }
    if (!ParseModuleListStream(minidumpParser)) {
        DFXLOGE("Failed to parse module list stream");
    }
    if (!ParseMemoryListStream(minidumpParser)) {
        DFXLOGE("Failed to parse memory list stream");
    }
    if (!ParseMapListStream(minidumpParser)) {
        DFXLOGE("Failed to parse map list stream");
    }
    auto endTime = std::chrono::steady_clock::now();
    auto parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();

    DFXLOGI("Parse completed in %{public}llu ms", parseTime);

    minidumpParser.PrintPerformanceStats();
    return true;
}

std::shared_ptr<DfxRegs> MinidumpDumper::CreateARM64DfxRegs(std::shared_ptr<MDRawContextARM64> arm64Context)
{
    if (arm64Context == nullptr) {
        DFXLOGE("arm64Context is nullptr");
        return nullptr;
    }
    auto regs = arm64Context->iregs;
    auto dfxRegs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, reinterpret_cast<uintptr_t*>(regs),
        MD_CONTEXT_ARM64_GPR_COUNT);
    if (dfxRegs == nullptr) {
        DFXLOGE("dfxRegs is nullptr");
        return nullptr;
    }
    auto cpsr = static_cast<uintptr_t>(arm64Context->cpsr);
    dfxRegs->SetReg(MD_CONTEXT_ARM64_GPR_COUNT, &cpsr);
    return dfxRegs;
}

bool MinidumpDumper::BuildSignalInfoFromException(MinidumpException* exception)
{
    if (exception == nullptr) {
        DFXLOGE("exception is nullptr");
        return false;
    }
    auto exceptionRecord = exception->ExceptionRecord();
    if (exceptionRecord == nullptr) {
        DFXLOGE("exceptionRecord is nullptr");
        return false;
    }
    siginfo_t sigInfo {
        .si_signo = exceptionRecord->exceptionCode,
        .si_code = exceptionRecord->exceptionFlags,
        .si_addr = reinterpret_cast<void*>(exceptionRecord->exceptionAddress),
    };
    process_.SetReason(DfxSignal::PrintSignal(sigInfo));
    return true;
}

bool MinidumpDumper::ParseExceptionStream(MinidumpParser& minidumpParser)
{
    MinidumpException* exception = minidumpParser.GetException();
    if (exception == nullptr) {
        DFXLOGE("exception is nullptr");
        return false;
    }
    auto exceptionStream = exception->Exception();
    if (exceptionStream == nullptr) {
        DFXLOGE("exceptionStream is nullptr");
        return false;
    }
    auto keyThreadId = exceptionStream->threadId;
    auto keyThread = DfxThread::Create(request_.pid, keyThreadId, keyThreadId, false);
    if (keyThread == nullptr) {
        DFXLOGE("keyThread is nullptr");
        return false;
    }
    if (!BuildSignalInfoFromException(exception)) {
        return false;
    }
    auto context = exception->GetContext();
    if (context == nullptr) {
        DFXLOGE("context is nullptr");
        return false;
    }
    auto faultThreadRegs = CreateARM64DfxRegs(context->GetContextARM64());
    if (faultThreadRegs == nullptr) {
        return false;
    }
    auto* esrInfo = minidumpParser.GetEsrInfo();
    if (esrInfo != nullptr) {
        auto esr = static_cast<uintptr_t>(esrInfo->EsrRegsInfo().esrRegs);
        faultThreadRegs->SetReg(MD_CONTEXT_ARM64_GPR_COUNT + 1, &esr);
    }
    process_.SetFaultThreadRegisters(faultThreadRegs);
    auto dfxRegs = CreateARM64DfxRegs(context->GetContextARM64());
    if (dfxRegs == nullptr) {
        return false;
    }
    keyThread->SetThreadRegs(dfxRegs);
    process_.SetKeyThread(keyThread);
    return true;
}

bool MinidumpDumper::ParseThreadNameStream(MinidumpParser& minidumpParser)
{
    MinidumpThreadNameList* threadNameList = minidumpParser.GetThreadNameList();
    if (threadNameList == nullptr) {
        DFXLOGE("threadNameList is nullptr");
        return false;
    }
    auto threadNames = threadNameList->GetThreadNames();
    auto keyThread = process_.GetKeyThread();
    if (keyThread == nullptr) {
        DFXLOGE("keyThread is nullptr");
        return false;
    }
    auto keyThreadId = keyThread->GetThreadInfo().tid;
    for (const auto& threadName : threadNames) {
        uint32_t tid = 0;
        threadName->GetThreadID(tid);
        std::string name = threadName->GetThreadName();
        if (tid == static_cast<uint32_t>(keyThreadId)) {
            keyThread->SetThreadName(name);
            continue;
        }
        auto thread = DfxThread::Create(request_.pid, tid, tid, false);
        thread->SetThreadName(name);
        process_.GetOtherThreads().emplace_back(std::move(thread));
    }
    return true;
}

bool MinidumpDumper::ParseModuleListStream(MinidumpParser& minidumpParser)
{
    MinidumpModuleList* moduleList = minidumpParser.GetModuleList();
    dfxMaps_ = std::make_shared<DfxMaps>();
    if (moduleList == nullptr) {
        DFXLOGE("moduleList is nullptr");
        return false;
    }
    DFXLOGI("Module count: %{public}u", moduleList->ModuleCount());
    auto modules = moduleList->GetModules();
    for (const auto& module : modules) {
        if (module == nullptr) {
            DFXLOGE("module is nullptr");
            continue;
        }
        auto map = std::make_shared<DfxMap>(module->BaseAddress(),
            module->BaseAddress() + module->Size(), 0, PROT_READ | PROT_WRITE | PROT_EXEC, module->CodeFile());
        if (map == nullptr) {
            DFXLOGE("map is nullptr");
            continue;
        }
        dfxMaps_->AddMap(map);
    }
    return true;
}

bool MinidumpDumper::ParseMemoryListStream(MinidumpParser& minidumpParser)
{
    MinidumpMemoryList* memoryList = minidumpParser.GetMemoryList();
    if (memoryList == nullptr) {
        DFXLOGE("memoryList is nullptr");
        return false;
    }
    auto regs = process_.GetFaultThreadRegisters();
    if (regs == nullptr) {
        DFXLOGE("regs is nullptr");
        return false;
    }
    auto& memoryIns = MemoryNearRegisterUtil::GetInstance();
    for (const auto& regData : regs->GetRegsData()) {
        MemoryBlockInfo info;
        info.nameAddr = regData;
        info.startAddr = info.nameAddr - COMMON_REG_MEM_FORWARD_SIZE * sizeof(uintptr_t);
        for (auto addr = info.startAddr; addr < info.startAddr + COMMON_REG_MEM_SIZE * sizeof(uintptr_t);
            addr += sizeof(uintptr_t)) {
            uintptr_t value = 0;
            auto memoryRegion = memoryList->GetMemoryRegionForAddress(addr);
            if (memoryRegion == nullptr || !memoryRegion->GetMemory() ||
                !memoryRegion->GetMemoryAtAddress(addr, value)) {
                value = static_cast<uintptr_t>(-1);
            }
            info.content.emplace_back(value);
        }
        info.size = info.content.size();
        memoryIns.blocksInfo_.emplace_back(std::move(info));
    }
    DFXLOGI("Memory region count: %{public}u", memoryList->RegionCount());
    return true;
}

bool MinidumpDumper::ParseMapListStream(MinidumpParser& minidumpParser)
{
    MinidumpMapList* mapList = minidumpParser.GetMapList();
    if (mapList == nullptr) {
        DFXLOGE("mapList is nullptr");
        return false;
    }
    auto mapContent = mapList->GetContents();
    std::string mapBuffer(mapContent.begin(), mapContent.end());
    AppExecFwk::BundleMgrClient client;
    std::string bundleName;
    if (client.GetNameForUid(request_.uid, bundleName) != ERR_OK) {
        DFXLOGW("Failed to query bundleName from bms, uid:%{public}d.", request_.uid);
    } else {
        DFXLOGI("bundleName of uid:%{public}u is %{public}s", request_.uid, bundleName.c_str());
    }
    dfxMaps_ = DfxMaps::CreateByBuffer(bundleName, mapBuffer);
    return true;
}

bool MinidumpDumper::SetupKeyThreadStack(MinidumpThreadList* threadList, std::shared_ptr<DfxThread> keyThread)
{
    if (threadList == nullptr) {
        DFXLOGE("threadList is nullptr");
        return false;
    }
    auto minidumpKeyThread = threadList->GetThreadByID(keyThread->GetThreadInfo().tid);
    if (minidumpKeyThread == nullptr) {
        DFXLOGE("minidumpKeyThread is nullptr");
        return false;
    }
    auto memory = minidumpKeyThread->GetMemory();
    if (memory == nullptr) {
        DFXLOGE("memory is nullptr");
        return false;
    }
    auto keyStackBuf = memory->GetMemory();
    if (keyStackBuf == nullptr) {
        DFXLOGE("keyStackBuf is nullptr");
        return false;
    }
    keyThread->SetThreadStackBuffer(keyStackBuf);
    keyThread->SetStartOfStackMemory(minidumpKeyThread->GetStartOfStackMemoryRange());
    return true;
}

void MinidumpDumper::PopulateOtherThreadFromMinidump(std::shared_ptr<DfxThread> dfxThread,
    MinidumpThreadList* threadList)
{
    if (dfxThread == nullptr) {
        DFXLOGE("dfxThread is nullptr");
        return;
    }
    dfxThread->GetThreadRegs();
    if (threadList == nullptr) {
        DFXLOGE("threadList is nullptr");
        return;
    }
    auto minidumpThread = threadList->GetThreadByID(dfxThread->GetThreadInfo().tid);
    if (minidumpThread == nullptr) {
        DFXLOGE("minidumpThread is nullptr");
        return;
    }
    auto context = minidumpThread->GetContext();
    if (context == nullptr) {
        DFXLOGE("context is nullptr");
        return;
    }
    auto dfxRegs = CreateARM64DfxRegs(context->GetContextARM64());
    if (dfxRegs == nullptr) {
        return;
    }
    dfxThread->SetThreadRegs(dfxRegs);
    auto memoryRegion = minidumpThread->GetMemory();
    if (memoryRegion == nullptr) {
        DFXLOGE("memory is nullptr");
        return;
    }
    auto stackBuf = memoryRegion->GetMemory();
    if (stackBuf == nullptr) {
        DFXLOGE("stackBuf is nullptr");
        return;
    }
    dfxThread->SetThreadStackBuffer(stackBuf);
    dfxThread->SetStartOfStackMemory(minidumpThread->GetStartOfStackMemoryRange());
}

bool MinidumpDumper::ParseThreadListStream(MinidumpParser& minidumpParser)
{
    MinidumpThreadList* threadList = minidumpParser.GetThreadList();
    if (threadList == nullptr) {
        DFXLOGE("threadList is nullptr");
        return false;
    }
    auto keyThread = process_.GetKeyThread();
    if (keyThread == nullptr) {
        DFXLOGE("keyThread is nullptr");
        return false;
    }
    if (!SetupKeyThreadStack(threadList, keyThread)) {
        return false;
    }
    for (const auto& dfxThread : process_.GetOtherThreads()) {
        PopulateOtherThreadFromMinidump(dfxThread, threadList);
    }
    DFXLOGI("Thread count: %{public}u", threadList->ThreadCount());
    return true;
}

bool MinidumpDumper::TransferData(int srcFd, int dstFd)
{
    std::vector<char> buffer(BUFFER_SIZE);

    ssize_t totoBytesRead = 0;
    ssize_t bytesRead;

    while ((bytesRead = read(srcFd, buffer.data(), buffer.size())) > 0) {
        totoBytesRead += bytesRead;
        ssize_t bytesWritten = 0;
        while (bytesWritten < bytesRead) {
            ssize_t ret = write(dstFd, buffer.data() + bytesWritten, bytesRead - bytesWritten);
            if (ret < 0) {
                DFXLOGE("write failed, errno=%{public}d", errno);
                return false;
            }
            bytesWritten += ret;
        }
    }

    if (bytesRead < 0) {
        DFXLOGE("read failed, errno=%{public}d", errno);
        return false;
    }

    DFXLOGI("totol read form srcFd=%{public}d totoBytesRead=%{public}zd", srcFd, totoBytesRead);
    return true;
}

bool MinidumpDumper::GenerateMinidump(int pid, pid_t pipeFd, bool enableMinidump)
{
    if (pipeFd < 0) {
        DFXLOGE("invalid pipeFd=%{public}d", pipeFd);
        return false;
    }
    SmartFd pipeGuard(pipeFd);
    struct FaultLoggerdRequest faultloggerdRequest{
        .pid = request_.pid,
        .type = FaultLoggerType::MINIDUMP,
        .tid = request_.pid,
        .time = request_.timeStamp,
        .minidump = enableMinidump
    };
    outputFdGuard_ = SmartFd {RequestFileDescriptorEx(&faultloggerdRequest)};
    if (!outputFdGuard_) {
        DFXLOGW("Failed to request_ fd from faultloggerd.");
        return false;
    }

    DFXLOGI("created temp file for minidump: pid=%{public}d", pid);

    if (!TransferData(pipeFd, outputFdGuard_.GetFd())) {
        DFXLOGE("failed to transfer minidump data for pid=%{public}d", pid);
        return false;
    }

    DFXLOGI("successfully transferred minidump data for pid=%{public}d", pid);
    return true;
}

void MinidumpDumper::UnwindProcess()
{
    if (dfxMaps_ == nullptr) {
        DFXLOGE("dfxMaps is nullptr");
        return;
    }
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.SetMaps(dfxMaps_);
    instance.SetStackForward(0);
    auto keyThread = process_.GetKeyThread();
    if (keyThread == nullptr) {
        DFXLOGE("keyThread is nullptr");
        return;
    }
    auto stackBuf = keyThread->GetThreadStackBuffer();
    if (stackBuf == nullptr) {
        DFXLOGE("stackBuf is nullptr");
        return;
    }
    instance.SetStackBuf(*stackBuf);
    instance.SetSp(keyThread->GetStartOfStackMemory());
    unwinder_ = std::make_shared<Unwinder>(instance.CreateAccessors());
    unwinder_->EnableFillFrames(true);
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    auto keyThreadRegs = keyThread->GetThreadRegs();
    if (keyThreadRegs == nullptr) {
        DFXLOGE("keyThreadRegs is nullptr");
        return;
    }
    unwinder_->SetRegs(keyThreadRegs);
    unwinder_->SetMaps(dfxMaps_);
    unwinder_->Unwind(&request_.context);
    keyThread->SetFrames(unwinder_->GetFrames());
    faultStack_.SetLiteType(true);
    faultStack_.Collect(process_, request_, *unwinder_);
    UnwindOtherThread();
}

void MinidumpDumper::UnwindOtherThread()
{
    if (unwinder_ == nullptr) {
        DFXLOGE("unwinder_ is nullptr");
        return;
    }
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.SetStackForward(0);
    for (const auto& dfxThread : process_.GetOtherThreads()) {
        if (dfxThread == nullptr) {
            DFXLOGE("dfxThread is nullptr");
            continue;
        }
        auto stackBuf = dfxThread->GetThreadStackBuffer();
        if (stackBuf == nullptr) {
            DFXLOGE("stackBuf is nullptr");
            continue;
        }
        instance.SetStackBuf(*stackBuf);
        instance.SetSp(dfxThread->GetStartOfStackMemory());
        auto dfxThreadRegs = dfxThread->GetThreadRegs();
        if (dfxThreadRegs == nullptr) {
            DFXLOGE("dfxThreadRegs is nullptr");
            continue;
        }
        unwinder_->SetRegs(dfxThreadRegs);
        unwinder_->Unwind(&request_.context);
        dfxThread->SetFrames(unwinder_->GetFrames());
    }
}

void MinidumpDumper::PrintDumpInfo()
{
    PrintHeader();
    PrintThreadInfo();
    PrintRegisters();
    PrintOtherThreadInfo();
    PrintRegsNearMemory();
    PrintFaultStack();
    PrintMaps();
}

void MinidumpDumper::PrintHeader()
{
    std::string tempStr = DumpUtils::GetBuildInfo();
    CppCrashInfoCollector::Instance().SetBuildInfo(tempStr);
    std::string headInfo = StringPrintf("Build info:%s\n", tempStr.c_str());
    tempStr = GetCurrentTimeStr(request_.timeStamp);
    headInfo += "Timestamp:" + tempStr;
    CppCrashInfoCollector::Instance().SetTimestamp(tempStr);

    headInfo += StringPrintf("Pid:%d\n", request_.pid);
    CppCrashInfoCollector::Instance().SetPid(request_.pid);
    headInfo += StringPrintf("Uid:%d\n", request_.uid);
    CppCrashInfoCollector::Instance().SetUid(request_.uid);
    headInfo += StringPrintf("Process name:%s\n", request_.processName);
    CppCrashInfoCollector::Instance().SetPname(request_.processName);

    tempStr = std::to_string(process_.GetLifeTime()) + "s";
    headInfo += "Process life time:" + tempStr + "\n";
    CppCrashInfoCollector::Instance().SetProcessLifeTime(tempStr);

    tempStr = StringPrintf("%" PRIu64 "(Rss)", process_.GetRss());
    headInfo += "Process Memory(kB): " + tempStr + "\n";
    CppCrashInfoCollector::Instance().SetProcessRssMeminfo(tempStr);

    std::string reason = process_.GetReason() + "\n";
    headInfo += "Reason:" + reason;
    CppCrashInfoCollector::Instance().SetReason(reason);

    DfxBufferWriter::GetInstance().WriteMsg(headInfo);
    DFXLOGI("%{public}s", headInfo.c_str());
}

void MinidumpDumper::PrintThreadInfo()
{
    if (unwinder_ == nullptr) {
        return;
    }
    std::string faultThreadInfo;
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        faultThreadInfo += "Fault thread info:\n";
    }
    auto keyThread = process_.GetKeyThread();
    if (keyThread == nullptr) {
        return;
    }
    faultThreadInfo += keyThread->ToString();
    CppCrashInfoCollector::Instance().SetKeyThread(keyThread->GetThreadInfo().threadName,
        keyThread->GetThreadInfo().tid, keyThread->GetFrames());

    auto& instance = DfxBufferWriter::GetInstance();
    instance.WriteMsg(faultThreadInfo);
    
    std::istringstream iss(faultThreadInfo);
    std::string line;
    while (std::getline(iss, line)) {
        DFXLOGI("%{public}s", line.c_str());
    }
}

void MinidumpDumper::PrintRegisters()
{
    auto regs = process_.GetFaultThreadRegisters();
    if (regs == nullptr) {
        return;
    }
    auto& instance = DfxBufferWriter::GetInstance();
    std::string regsStr = regs->PrintRegs();
    std::string prefix = "Registers:\n";
    std::string regsSubStr;
    if (regsStr.substr(0, prefix.size()) == prefix) {
        regsSubStr = regsStr.substr(prefix.size());
    } else {
        regsSubStr = regsStr;
    }
    CppCrashInfoCollector::Instance().SetRegisters(regsSubStr);
    instance.WriteMsg(regsStr);
    std::istringstream iss(regsStr);
    std::string line;
    while (std::getline(iss, line)) {
        DFXLOGI("%{public}s", line.c_str());
    }
}

void MinidumpDumper::PrintOtherThreadInfo()
{
    std::string otherThreadInfo;
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        otherThreadInfo += "Other thread info:\n";
    }
    for (const auto& thread : process_.GetOtherThreads()) {
        otherThreadInfo += thread->ToString();
        CppCrashInfoCollector::Instance().AddOtherThread(thread->GetThreadInfo().threadName,
            thread->GetThreadInfo().tid, thread->GetFrames());
    }
    auto& instance = DfxBufferWriter::GetInstance();
    instance.WriteMsg(otherThreadInfo);
}

void MinidumpDumper::PrintRegsNearMemory()
{
    if (unwinder_ == nullptr) {
        return;
    }
    MemoryNearRegister nearRegMemory;
    nearRegMemory.SetLiteType(true);
    nearRegMemory.Collect(process_, request_, *unwinder_);
    nearRegMemory.Print(process_, request_, *unwinder_);
}

void MinidumpDumper::PrintFaultStack()
{
    if (unwinder_ == nullptr) {
        return;
    }
    faultStack_.Print(process_, request_, *unwinder_);
}

void MinidumpDumper::PrintMaps()
{
    if (dfxMaps_ == nullptr) {
        return;
    }
    std::string mapsStr;
    for (const auto &map : dfxMaps_->GetMaps()) {
        mapsStr.append(map->ToString());
    }
    CppCrashInfoCollector::Instance().SetMaps(mapsStr);
    DfxBufferWriter::GetInstance().WriteMsg("Maps:\n" + mapsStr);
}

void MinidumpDumper::Report()
{
    SysEventReporter reporter(request_.type);
    reporter.Report(process_, request_);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif