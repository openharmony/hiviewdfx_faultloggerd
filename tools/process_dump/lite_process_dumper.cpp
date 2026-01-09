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
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_exception.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "dfx_cutil.h"
#ifndef is_ohos_lite
#include "faultlog_client.h"
#endif
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#include "hisysevent_c.h"
#endif
#include "procinfo.h"
#include "reporter.h"
#include "thread_context.h"
#include "unwinder.h"
#include "unwinder_config.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "LiteProcessDumper"

namespace OHOS {
namespace HiviewDFX {

void LiteProcessDumper::ReadRequest(int pipeReadFd)
{
    if (read(pipeReadFd, &request_, sizeof(struct ProcessDumpRequest)) != sizeof(struct ProcessDumpRequest)) {
        DFXLOGI("failed to read request %{public}d", errno);
        return;
    }
    DFXLOGI("read remote request procname: %{public}s", request_.processName);
}

void LiteProcessDumper::ReadStat(int pipeReadFd)
{
    char buf[PROC_STAT_BUF_SIZE];
    if (read(pipeReadFd, buf, sizeof(buf)) != sizeof(buf)) {
        DFXLOGI("failed to read stat %{public}d", errno);
        return;
    }
    buf[PROC_STAT_BUF_SIZE - 1] = '\0';
    stat_ = buf;
    DFXLOGI("stat %{public}s", stat_.c_str());
}

void LiteProcessDumper::ReadStatm(int pipeReadFd)
{
    char buf[PROC_STATM_BUF_SIZE];
    if (read(pipeReadFd, buf, sizeof(buf)) != sizeof(buf)) {
        DFXLOGI("failed to read statm %{public}d", errno);
        return;
    }
    buf[PROC_STATM_BUF_SIZE - 1] = '\0';
    statm_ = buf;
    DFXLOGI("statm %{public}s", statm_.c_str());
}

void LiteProcessDumper::ReadStack(int pipeReadFd)
{
    stackBuf_.resize(PRIV_COPY_STACK_BUFFER_SIZE);
    if (read(pipeReadFd, stackBuf_.data(), PRIV_COPY_STACK_BUFFER_SIZE) != PRIV_COPY_STACK_BUFFER_SIZE) {
        DFXLOGE("read stack fail %{public}d", errno);
        return;
    }
    DFXLOGI("read stack success");
}

MemoryBlockInfo ReadSingleRegMem(int pipeReadFd, uintptr_t nameAddr, unsigned int count, unsigned int forward)
{
    std::vector<uintptr_t> bk(count);
    if (static_cast<unsigned long>(read(pipeReadFd, bk.data(), bk.size() * sizeof(uintptr_t))) !=
        bk.size() * sizeof(uintptr_t)) {
        DFXLOGI("failed to read reg near memory %{public}d", errno);
    }
    MemoryBlockInfo info;
    info.nameAddr = nameAddr;
    info.startAddr = info.nameAddr  - forward * sizeof(uintptr_t);
    info.size =  bk.size();
    info.content = bk;
    return info;
}

void LiteProcessDumper::ReadMemoryNearRegister(int pipeReadFd, ProcessDumpRequest request)
{
    constexpr int bufSize = 50;
    char data[bufSize];
    read(pipeReadFd, data, bufSize);
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
    read(pipeReadFd, data, bufSize);
    DFXLOGI("end memory tag %{public}s", data);
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
    ReadRequest(pipeReadFd);
    ReadStat(pipeReadFd);
    ReadStatm(pipeReadFd);
    ReadStack(pipeReadFd);
    ReadMemoryNearRegister(pipeReadFd, request_);

    constexpr int bufSize = 1024;
    char data[bufSize] = {0};
    ssize_t n = 1;
    while ((n = read(pipeReadFd, data, bufSize)) > 0) {
        rawData_ += std::string(data, n);
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
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    unwinder_->SetRegs(DfxRegs::CreateFromUcontext(request_.context));
    unwinder_->SetMaps(dfxMaps_);
    unwinder_->Unwind(&request_.context);
}

void LiteProcessDumper::InitProcess()
{
    process_ = std::make_shared<DfxProcess>();
    process_->InitProcessInfo(request_.pid, request_.nsPid, request_.uid, request_.processName);
    process_->SetFaultThreadRegisters(regs_);
    process_->InitKeyThread(request_, false);
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
    InitProcess();
    Unwind();
    PrintAll();
    Report();
    DFXLOGI("dump finish.");
    return true;
}

void LiteProcessDumper::PrintHeader()
{
    std::string buildInfo = "Unknown";
#ifndef is_ohos_lite
    buildInfo = OHOS::system::GetParameter("const.product.software.version", "Unknown");
#endif
    auto& instance = DfxBufferWriter::GetInstance();

    instance.WriteFormatMsg("Build info:%s\n", buildInfo.c_str());
    instance.WriteMsg("Timestamp:" + GetCurrentTimeStr(request_.timeStamp));
    instance.WriteFormatMsg("Pid:%d\n", request_.pid);
    instance.WriteFormatMsg("Uid:%d\n", request_.uid);
    instance.WriteFormatMsg("Process name:%s\n", request_.processName);

    uint64_t lifeTimeSeconds;
    GetProcessLifeCycle(stat_, lifeTimeSeconds);
    process_->SetLifeTime(lifeTimeSeconds);
    std::string tmp = "Process life time:" + std::to_string(lifeTimeSeconds) + "s" + "\n";
    instance.WriteMsg(tmp);

    uint64_t rss = GetProcRssMemInfo(statm_);
    process_->SetRss(rss);
    tmp = StringPrintf("Process Memory(kB): %" PRIu64 "(Rss)\n", rss);
    instance.WriteMsg(tmp);

    if (request_.siginfo.si_pid == request_.pid) {
        request_.siginfo.si_uid = request_.uid;
    }
    std::string reason = DfxSignal::PrintSignal(request_.siginfo) + "\n";
    process_->SetReason(DfxSignal::PrintSignal(request_.siginfo));
    instance.WriteMsg("Reason:");
    instance.WriteMsg(reason);
}

void LiteProcessDumper::PrintThreadInfo()
{
    if (unwinder_ == nullptr) {
        return;
    }
    auto& instance = DfxBufferWriter::GetInstance();
    std::string faultThreadInfo;
    faultThreadInfo += "Fault thread info:\n";
    faultThreadInfo += StringPrintf("Tid:%d, Name:%s\n", request_.tid, request_.threadName);
    keyThreadStackStr_ = unwinder_->GetFramesStr(unwinder_->GetFrames());
    faultThreadInfo += keyThreadStackStr_;
    instance.WriteMsg(faultThreadInfo);
    std::istringstream iss(faultThreadInfo);
    std::string line;
    while (std::getline(iss, line)) {
        DFXLOGI("%{public}s", line.c_str());
    }
}

void LiteProcessDumper::PrintRegisters()
{
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
    if (unwinder_ == nullptr) {
        return;
    }
    MemoryNearRegister nearRegMemory;
    nearRegMemory.SetLiteType(true);
    nearRegMemory.Print(*process_, request_, *unwinder_);
}

void LiteProcessDumper::PrintFaultStack()
{
    if (unwinder_ == nullptr) {
        return;
    }
    FaultStack faultStack;
    faultStack.SetLiteType(true);
    process_->GetKeyThread()->SetFrames(unwinder_->GetFrames());
    faultStack.Print(*process_, request_, *unwinder_);
}

void LiteProcessDumper::PrintMaps()
{
    if (dfxMaps_ == nullptr) {
        return;
    }
    std::string mapsStr = "Maps:\n";
    for (const auto &map : dfxMaps_->GetMaps()) {
        mapsStr.append(map->ToString());
    }
    DfxBufferWriter::GetInstance().WriteMsg(mapsStr);
}

void LiteProcessDumper::PrintOpenFiles()
{
    std::string openFiles = "OpenFiles:\n";
    DfxBufferWriter::GetInstance().WriteMsg(openFiles);

    std::istringstream iss(rawData_);
    std::string line;
    std::map<int, std::string> fdFiles;
    while (std::getline(iss, line)) {
        auto pos =  line.find_first_of('-');
        if (pos != std::string::npos) {
            long fd;
            if (!SafeStrtol(line.substr(0, pos).c_str(), &fd, DECIMAL_BASE)) {
                continue;
            }
            line += "\n";
            fdFiles.emplace(fd, line);
        }
    }
    for (auto& file : fdFiles) {
        DfxBufferWriter::GetInstance().WriteMsg(file.second);
    }
}

void LiteProcessDumper::PrintAll()
{
    PrintHeader();
    PrintThreadInfo();
    PrintRegisters();
    PrintRegsNearMemory();
    PrintFaultStack();
    PrintMaps();
    PrintOpenFiles();
    DFXLOGI("finish print all to file");
}

bool LiteProcessDumper::Report()
{
    if (DfxMaps::IsArkWebProc()) {
        SysEventReporter reporter(request_.type);
        reporter.Report(*process_, request_);
    }
    std::string reason = DfxSignal::PrintSignal(request_.siginfo) + "\n";
    std::string summary = "litehandler:" + reason + keyThreadStackStr_;
#ifndef HISYSEVENT_DISABLE
    HiSysEventParam params[] = {
        {.name = "PID", .t = HISYSEVENT_INT32, .v = { .i32 = request_.pid}, .arraySize = 0},
        {.name = "UID", .t = HISYSEVENT_INT32, .v = { .i32 = request_.uid}, .arraySize = 0},
        {.name = "PROCESS_NAME", .t = HISYSEVENT_STRING,
            .v = {.s = const_cast<char*>(request_.processName)}, .arraySize = 0},
        {.name = "HAPPEN_TIME", .t = HISYSEVENT_UINT64, .v = {.ui64 = GetTimeMilliSeconds()}, .arraySize = 0},
        {.name = "SUMMARY", .t = HISYSEVENT_STRING, .v = {.s = const_cast<char*>(summary.c_str())}, .arraySize = 0},
    };
    int ret = OH_HiSysEvent_Write("RELIABILITY", "CPP_CRASH_NO_LOG",
                                  HISYSEVENT_FAULT, params, sizeof(params) / sizeof(params[0]));
    DFXLOGI("Report pid %{public}d lite dump event ret %{public}d", request_.pid, ret);
    return ret == 0;
#else
    DFXLOGI("Not supported lite dump event report");
    return true;
#endif
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
