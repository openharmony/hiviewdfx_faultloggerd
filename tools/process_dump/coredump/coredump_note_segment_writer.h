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
#ifndef COREDUMP_NOTE_SEGMENT_WRITER_H
#define COREDUMP_NOTE_SEGMENT_WRITER_H

#include <sys/uio.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

#include "coredump_buffer_writer.h"
#include "coredump_common.h"
#include "coredump_config_manager.h"
#include "coredump_writer.h"
#include "dfx_log.h"
#include "dfx_regs.h"

namespace OHOS {
namespace HiviewDFX {
class NoteUtil {
public:
    bool NoteWrite(CoredumpBufferWriter& bw, uint32_t noteType, size_t descSize, const char* noteName);
};

class PrpsinfoWriter : public Writer {
public:
    PrpsinfoWriter(pid_t pid, CoredumpBufferWriter& bw) : pid_(pid), bw_(bw) {}
    bool Write() override;
private:
    void FillPrpsinfo(prpsinfo_t &ntPrpsinfo);
    bool ReadProcessStat(prpsinfo_t &ntPrpsinfo);
    bool ReadProcessStatus(prpsinfo_t &ntPrpsinfo);
    bool ReadProcessComm(prpsinfo_t &ntPrpsinfo);
    bool ReadProcessCmdline(prpsinfo_t &ntPrpsinfo);
    pid_t pid_;
    CoredumpBufferWriter& bw_;
};

template<typename T>
bool GetRegset(pid_t tid, int regsetType, T &regset)
{
    struct iovec iov;
    iov.iov_base = &regset;
    iov.iov_len = sizeof(T);

    if (ptrace(PTRACE_GETREGSET, tid, regsetType, &iov) == -1) {
        DFXLOGE("ptrace failed regsetType:%{public}d, tid:%{public}d, errno:%{public}d", regsetType, tid, errno);
        return false;
    }
    return true;
}

template<typename T>
bool GetSiginfoCommon(T &targetInfo, pid_t tid)
{
    if (ptrace(PTRACE_GETSIGINFO, tid, nullptr, &targetInfo) == -1) {
        DFXLOGE("ptrace failed PTRACE_GETSIGINFO, tid:%{public}d, errno:%{public}d", tid, errno);
        return false;
    }
    return true;
}

UserPacMask CoredumpGetPackMask(pid_t tid);
int CoredumpGetFpRegset(pid_t tid, struct user_fpsimd_struct& ntFpregset);
siginfo_t CoredumpGetSiginfo(pid_t tid);
int CoredumpGetPrstatus(pid_t tid, prstatus_t& ntPrstatus);

struct CoredumpThread {
    UserPacMask ntUserPacMask;
    struct user_fpsimd_struct ntFpregset;
    int fpRegValid;
    siginfo_t ntSiginfo;
    prstatus_t ntPrstatus;
    int prStatusValid;
    std::shared_ptr<DfxRegs> keyRegs_;

    void Init(pid_t tid, std::shared_ptr<DfxRegs> regs)
    {
        ntUserPacMask = CoredumpGetPackMask(tid);
        fpRegValid = CoredumpGetFpRegset(tid, ntFpregset);
        ntSiginfo = CoredumpGetSiginfo(tid);
        prStatusValid = CoredumpGetPrstatus(tid, ntPrstatus);
        keyRegs_ =  regs;
    }
};

/*
 * thread status and register, should include NT_ARM_PAC_MASK, lldb can backtrace success
 * ----------------------
 * |    NT_PRSTATUS    |  : General-purpose registers, thread status
 * ----------------------
 * |  NT_ARM_PAC_MASK  |  : Pointer authentication mask (PAC), required for unwinding
 * ----------------------
 * |     NT_FPREGSET   |  : Floating-point/SIMD registers
 * ----------------------
 * |     NT_SIGINFO    |  : Signal info (optional, but useful for crash context)
 * ----------------------
 * | ....               | : Additional notes (e.g., NT_ARM_TAGGED_ADDR_CTRL)
 * ----------------------
 */
class ThreadNoteWriter : public Writer {
public:
    ThreadNoteWriter(pid_t pid, pid_t targetTid, CoredumpBufferWriter& bw)
        : pid_(pid), targetTid_(targetTid), bw_(bw) {}
    bool Write() override;
    void SetKeyThreadData(CoredumpThread coreDumpKeyThreadData);
private:
    void ThreadNoteWrite(pid_t tid);
    bool PrstatusWrite(pid_t tid);
    bool ArmPacMaskWrite(pid_t tid);
    bool FpregsetWrite(pid_t tid);
    bool SiginfoWrite(pid_t tid);
    bool ArmTaggedAddrCtrlWrite();
    bool GetPrStatus(prstatus_t &ntPrstatus, pid_t tid);
    bool GetRusage(prstatus_t &ntPrstatus);
    bool GetPrReg(prstatus_t &ntPrstatus, pid_t tid);

    void InitWriters();
    using WriterFn = std::function<bool(pid_t)>;

    void RegisterThreadWriter(bool enable, WriterFn w)
    {
        if (enable) {
            writers_.push_back(std::move(w));
        }
    }

    pid_t pid_;
    pid_t targetTid_;
    CoredumpBufferWriter& bw_;
    CoredumpThread coredumpKeyThread_ {};
    std::vector<std::function<bool(pid_t)>> writers_;
    CoredumpConfig cfg_;
};

class MultiThreadNoteWriter : public Writer {
public:
    MultiThreadNoteWriter(pid_t pid, pid_t targetTid, CoredumpBufferWriter& bw, const CoredumpThread& coredumpThread)
        : pid_(pid), targetTid_(targetTid), bw_(bw), coredumpKeyThread_(coredumpThread) {}
    bool Write() override;
private:
    pid_t pid_;
    pid_t targetTid_;
    CoredumpBufferWriter& bw_;
    CoredumpThread coredumpKeyThread_;
};

/*
 * coredump PT_NOTE segment layout:
 * ---------------
 * |   PT_NOTE   |
 * ---------------
 * | NT_PRSTATUS |  // process status (registers, etc.)
 * ---------------
 * | ........... |  // thread register ..
 * ---------------
 * | NT_AUXV     |  // Auxiliary vector
 * --------------
 * | NT_FILE     |  // Mapped file info
 * ---------------
 */
class NoteSegmentWriter : public Writer {
public:
    NoteSegmentWriter(CoredumpProc& coreDumpThread, const std::vector<DumpMemoryRegions>& maps,
        CoredumpBufferWriter& bw)
        : pid_(coreDumpThread.targetPid), targetTid_(coreDumpThread.keyTid), maps_(maps), bw_(bw) {}
    bool Write() override;
    void SetKeyThreadData(CoredumpThread coredumpKeyThread);
    void InitWriters();
    void RegisterWriter(bool enabled, std::unique_ptr<Writer> writer)
    {
        if (enabled) {
            writers_.push_back(std::move(writer));
        }
    }
private:
    pid_t pid_;
    pid_t targetTid_;
    std::vector<DumpMemoryRegions> maps_;
    CoredumpThread coredumpKeyThread_ {};
    CoredumpBufferWriter& bw_;
    std::vector<std::unique_ptr<Writer>> writers_;
    CoredumpConfig cfg_;
};

class AuxvWriter : public Writer {
public:
    AuxvWriter(pid_t pid, CoredumpBufferWriter& bw) : pid_(pid), bw_(bw) {}
    bool Write() override;
private:
    bool ReadProcessAuxv(Elf64_Nhdr *note);
    pid_t pid_;
    CoredumpBufferWriter& bw_;
};

class FileRegionWriter : public Writer {
public:
    FileRegionWriter(std::vector<DumpMemoryRegions>& maps, CoredumpBufferWriter& bw) : maps_(maps), bw_(bw) {}
    bool Write() override;
private:
    bool WriteAddrRelated();
    bool WriteFilePath(Elf64_Half &lineNumber);
    std::vector<DumpMemoryRegions> maps_;
    CoredumpBufferWriter& bw_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
