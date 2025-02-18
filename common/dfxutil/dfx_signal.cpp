/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include <map>
#include <string>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxSignal"
}

bool DfxSignal::IsAvailable() const
{
    struct sigaction prevAction;
    if (sigaction(signal_, nullptr, &prevAction) < 0) {
        return 0;
    }
    return static_cast<unsigned int>(prevAction.sa_flags) & SA_SIGINFO;
}

bool DfxSignal::IsAddrAvailable() const
{
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
}

bool DfxSignal::IsPidAvailable() const
{
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

std::string DfxSignal::PrintSignal(const siginfo_t &info)
{
    std::string sigString = StringPrintf("Signal:%s(%s)", FormatSignalName(info.si_signo).c_str(),\
        FormatCodeName(info.si_signo, info.si_code).c_str());

    DfxSignal signal(info.si_signo);
    if (signal.IsAddrAvailable()) {
        sigString = sigString + StringPrintf("@%" PRIX64_ADDR " ", (uint64_t)info.si_addr);
    }

    if ((info.si_code <= 0) && (info.si_pid != 0)) {
        sigString = sigString + StringPrintf("from:%d:%u", info.si_pid, info.si_uid);
    }

    return sigString;
}

std::string DfxSignal::FormatSignalName(const int32_t signal)
{
    std::map<int32_t, std::string> sigMaps = {
        { SIGILL, std::string("SIGILL") },
        { SIGTRAP, std::string("SIGTRAP") },
        { SIGABRT, std::string("SIGABRT") },
        { SIGALRM, std::string("SIGALRM") },
        { SIGBUS, std::string("SIGBUS") },
        { SIGFPE, std::string("SIGFPE") },
        { SIGSEGV, std::string("SIGSEGV") },
        { SIGSTKFLT, std::string("SIGSTKFLT") },
        { SIGSYS, std::string("SIGSYS") },
        { SIGDUMP, std::string("SIGDUMP") },
        { SIGLEAK_STACK, std::string("DEBUG SIGNAL") },
    };

    if (sigMaps.find(signal) != sigMaps.end()) {
        return sigMaps[signal];
    }
    return "Uncare Signal";
}

std::string DfxSignal::FormatCodeName(const int32_t signal, const int32_t signalCode)
{
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
        case SIGSYS:
            return FormatSIGSYSCodeName(signalCode);
        case SIGLEAK_STACK:
            return FormatSIGLEAKCodeName(signalCode);
        default:
            break;
    }
    return DfxSignal::FormatCommonSignalCodeName(signalCode);
}

std::string DfxSignal::FormatSIGBUSCodeName(const int32_t signalCode)
{
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

std::string DfxSignal::FormatSIGILLCodeName(const int32_t signalCode)
{
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

std::string DfxSignal::FormatSIGFPECodeName(const int32_t signalCode)
{
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

std::string DfxSignal::FormatSIGSEGVCodeName(const int32_t signalCode)
{
    switch (signalCode) {
        case SEGV_MAPERR:
            return "SEGV_MAPERR";
        case SEGV_ACCERR:
            return "SEGV_ACCERR";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string DfxSignal::FormatSIGTRAPCodeName(const int32_t signalCode)
{
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

std::string DfxSignal::FormatSIGSYSCodeName(const int32_t signalCode)
{
    switch (signalCode) {
        case SYS_SECCOMP:
            return "SYS_SECCOMP";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string DfxSignal::FormatSIGLEAKCodeName(const int32_t signalCode)
{
    switch (signalCode) {
        case SIGLEAK_STACK_FDSAN:
            return "FDSAN";
        case SIGLEAK_STACK_JEMALLOC:
            return "JEMALLOC";
        case SIGLEAK_STACK_BADFD:
            return "BADFD";
        default:
            return FormatCommonSignalCodeName(signalCode);
    }
}

std::string DfxSignal::FormatCommonSignalCodeName(const int32_t signalCode)
{
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
