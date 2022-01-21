/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "dfx_signal.h"

#include <csignal>
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
DfxSignal::DfxSignal(const int32_t signal)
{
    signal_ = signal;
}

bool DfxSignal::IsAvaliable() const
{
    DfxLogDebug("Enter %s.", __func__);
    struct sigaction previousAction;
    if (sigaction(signal_, nullptr, &previousAction) < 0) {
        return 0;
    }
    DfxLogDebug("Exit %s.", __func__);
    return previousAction.sa_flags & SA_SIGINFO;
}

bool DfxSignal::IsAddrAvaliable() const
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signal_) {
        case SIGABRT:
        case SIGBUS:
        case SIGILL:
        case SIGSEGV:
        case SIGTRAP:
            return true;
        default:
            return false;
    }
    DfxLogDebug("Exit %s.", __func__);
}

bool DfxSignal::IsPidAvaliable() const
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signal_) {
        case SI_USER:
        case SI_QUEUE:
        case SI_TIMER:
        case SI_ASYNCIO:
        case SI_MESGQ:
            return true;
        default:
            return false;
    }
}

int32_t DfxSignal::GetSignal() const
{
    return signal_;
}

void PrintSignal(const siginfo_t &info, const int32_t fd)
{
    DfxLogDebug("Enter %s.", __func__);
    WriteLog(fd, "Signal:%s(%s)", FormatSignalName(info.si_signo).c_str(),
        FormatCodeName(info.si_signo, info.si_code).c_str());

    DfxSignal signal(info.si_signo);
    if (signal.IsAddrAvaliable()) {
#if defined(__aarch64__)
        WriteLog(fd, "@0x%016lx ", (uint64_t)info.si_addr);
#elif defined(__arm__)
        WriteLog(fd, "@0x%08x ", (uint32_t)info.si_addr);
#elif defined(__x86_64__)
        WriteLog(fd, "@0x%016lx ", static_cast<uint64_t>(info.si_addr));
#else
#pragma message("Unsupport arch.")
#endif
    }

    if ((info.si_code <= 0) && (info.si_pid != 0)) {
        WriteLog(fd, "from:%d:%d", info.si_pid, info.si_uid);
    }

    WriteLog(fd, "\n");
    DfxLogDebug("Exit %s.", __func__);
}

std::string FormatSignalName(const int32_t signal)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signal) {
        case SIGABRT:
            return "SIGABRT";
        case SIGALRM:
            return "SIGALRM";
        case SIGBUS:
            return "SIGBUS";
        case SIGFPE:
            return "SIGFPE";
        case SIGILL:
            return "SIGILL";
        case SIGSEGV:
            return "SIGSEGV";
        case SIGSYS:
            return "SIGSYS";
        case SIGTRAP:
            return "SIGTRAP";
        default:
            return "Uncare Signal";
    }
}

std::string FormatCodeName(const int32_t signal, const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signal) {
        case SIGILL:
            return FormatSIGILLCodeName(signalCode);
        case SIGBUS:
            return FormatSIGBUSCodeName(signalCode);
        case SIGFPE:
            return FormatSIGFPECodeName(signalCode);
        case SIGSEGV:
            return FormatSIGSEGVCodeName(signalCode);
        case SIGTRAP:
            return FormatSIGTRAPCodeName(signalCode);
        default:
            break;
    }
    return FormatCommonSignalCodeName(signalCode);
}

std::string FormatSIGBUSCodeName(const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signalCode) {
        case BUS_ADRALN:
            return "BUS_ADRALN";
        case BUS_ADRERR:
            return "BUS_ADRERR";
        case BUS_OBJERR:
            return "BUS_OBJERR";
        case BUS_MCEERR_AR:
            return "BUS_MCEERR_AR";
        case BUS_MCEERR_AO:
            return "BUS_MCEERR_AO";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string FormatSIGILLCodeName(const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signalCode) {
        case ILL_ILLOPC:
            return "ILL_ILLOPC";
        case ILL_ILLOPN:
            return "ILL_ILLOPN";
        case ILL_ILLADR:
            return "ILL_ILLADR";
        case ILL_ILLTRP:
            return "ILL_ILLTRP";
        case ILL_PRVOPC:
            return "ILL_PRVOPC";
        case ILL_PRVREG:
            return "ILL_PRVREG";
        case ILL_COPROC:
            return "ILL_COPROC";
        case ILL_BADSTK:
            return "ILL_BADSTK";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string FormatSIGFPECodeName(const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signalCode) {
        case FPE_INTDIV:
            return "FPE_INTDIV";
        case FPE_INTOVF:
            return "FPE_INTOVF";
        case FPE_FLTDIV:
            return "FPE_FLTDIV";
        case FPE_FLTOVF:
            return "FPE_FLTOVF";
        case FPE_FLTUND:
            return "FPE_FLTUND";
        case FPE_FLTRES:
            return "FPE_FLTRES";
        case FPE_FLTINV:
            return "FPE_FLTINV";
        case FPE_FLTSUB:
            return "FPE_FLTSUB";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string FormatSIGSEGVCodeName(const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signalCode) {
        case SEGV_MAPERR:
            return "SEGV_MAPERR";
        case SEGV_ACCERR:
            return "SEGV_ACCERR";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string FormatSIGTRAPCodeName(const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signalCode) {
        case TRAP_BRKPT:
            return "TRAP_BRKPT";
        case TRAP_TRACE:
            return "TRAP_TRACE";
        case TRAP_BRANCH:
            return "TRAP_BRANCH";
        case TRAP_HWBKPT:
            return "TRAP_HWBKPT";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string FormatCommonSignalCodeName(const int32_t signalCode)
{
    DfxLogDebug("Enter %s.", __func__);
    switch (signalCode) {
        case SI_USER:
            return "SI_USER";
        case SI_KERNEL:
            return "SI_KERNEL";
        case SI_QUEUE:
            return "SI_QUEUE";
        case SI_TIMER:
            return "SI_TIMER";
        case SI_MESGQ:
            return "SI_MESGQ";
        case SI_ASYNCIO:
            return "SI_ASYNCIO";
        case SI_SIGIO:
            return "SI_SIGIO";
        case SI_TKILL:
            return "SI_TKILL";
        default:
            return "UNKNOWN";
    }
}
} // namespace HiviewDFX
} // namespace OHOS
