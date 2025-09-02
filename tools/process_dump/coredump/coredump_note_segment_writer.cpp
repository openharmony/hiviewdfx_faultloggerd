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
#include "coredump_note_segment_writer.h"

#include <dirent.h>
#include <fstream>
#include <sys/resource.h>

#include "file_util.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr const char * const NOTE_NAME_CORE = "CORE";
    constexpr const char * const NOTE_NAME_LINUX = "LINUX";
    constexpr const char * const UID = "Uid:";
    constexpr const char * const GID = "Gid:";
    constexpr const char * const PPID = "PPid:";
    constexpr const char * const SIGPND = "SigPnd:";
    constexpr const char * const SIGBLK = "SigBlk:";
    constexpr int NOTE_NAME_SIZE = 8;
}

void NoteSegmentWriter::InitWriters()
{
    RegisterWriter(cfg_.process.prpsinfo, std::make_unique<PrpsinfoWriter>(pid_, bw_));
    RegisterWriter(cfg_.process.multiThread, std::make_unique<MultiThreadNoteWriter>(
        pid_, targetTid_, bw_, coredumpKeyThread_));
    RegisterWriter(cfg_.process.auxv, std::make_unique<AuxvWriter>(pid_, bw_));
    RegisterWriter(cfg_.process.dumpMappings, std::make_unique<FileRegionWriter>(maps_, bw_));
}

bool NoteSegmentWriter::Write()
{
    char *noteStart = bw_.GetCurrent();
    InitWriters();
    for (auto& writer: writers_) {
        writer->Write();
    }

    Elf64_Phdr *ptNote = reinterpret_cast<Elf64_Phdr *>(bw_.GetBase() + sizeof(Elf64_Ehdr));
    ptNote->p_filesz = reinterpret_cast<uintptr_t>(bw_.GetCurrent()) - reinterpret_cast<uintptr_t>(noteStart);
    ptNote->p_offset = reinterpret_cast<uintptr_t>(noteStart) - reinterpret_cast<uintptr_t>(bw_.GetBase());
    return true;
}

bool ThreadNoteWriter::Write()
{
    InitWriters();
    ThreadNoteWrite(pid_);
    return true;
}

void ThreadNoteWriter::InitWriters()
{
    RegisterThreadWriter(cfg_.threads.prstatus, [this](pid_t tid) {return PrstatusWrite(tid);});
    RegisterThreadWriter(cfg_.threads.armPacMask, [this](pid_t tid) {return ArmPacMaskWrite(tid);});
    RegisterThreadWriter(cfg_.threads.fpregset, [this](pid_t tid) {return FpregsetWrite(tid);});
    RegisterThreadWriter(cfg_.threads.siginfo, [this](pid_t tid) {return SiginfoWrite(tid);});
    RegisterThreadWriter(cfg_.threads.armTaggedAddrCtrl, [this](pid_t tid) {return ArmTaggedAddrCtrlWrite();});
}

void ThreadNoteWriter::ThreadNoteWrite(pid_t tid)
{
    for (auto& noteUnitWrite : writers_) {
        noteUnitWrite(tid);
    }
}

bool ThreadNoteWriter::ArmPacMaskWrite(pid_t tid)
{
    if (!NoteUtil().NoteWrite(bw_, NT_ARM_PAC_MASK, sizeof(UserPacMask), NOTE_NAME_LINUX)) {
        return false;
    }

    UserPacMask ntUserPacMask;
    if (tid == targetTid_) {
        ntUserPacMask = coredumpKeyThread_.ntUserPacMask;
    } else {
        ntUserPacMask = CoredumpGetPackMask(tid);
    }
    if (!bw_.Write(&ntUserPacMask, sizeof(ntUserPacMask))) {
        DFXLOGE("Write ntUserPacMask fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool ThreadNoteWriter::PrstatusWrite(pid_t tid)
{
    if (!NoteUtil().NoteWrite(bw_, NT_PRSTATUS, sizeof(prstatus_t), NOTE_NAME_CORE)) {
        return false;
    }
    prstatus_t ntPrstatus;
    GetPrStatus(ntPrstatus, tid);
    GetPrReg(ntPrstatus, tid);
    if (!bw_.Write(&ntPrstatus, sizeof(ntPrstatus))) {
        DFXLOGE("Write ntPrstatus fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool ThreadNoteWriter::FpregsetWrite(pid_t tid)
{
    if (!NoteUtil().NoteWrite(bw_, NT_FPREGSET, sizeof(struct user_fpsimd_struct), NOTE_NAME_CORE)) {
        return false;
    }

    struct user_fpsimd_struct ntFpregset;
    if (tid == targetTid_) {
        ntFpregset = coredumpKeyThread_.ntFpregset;
    } else {
        CoredumpGetFpRegset(tid, ntFpregset);
    }

    if (!bw_.Write(&ntFpregset, sizeof(ntFpregset))) {
        DFXLOGE("Write ntFpregset fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool ThreadNoteWriter::SiginfoWrite(pid_t tid)
{
    if (!NoteUtil().NoteWrite(bw_, NT_SIGINFO, sizeof(siginfo_t), NOTE_NAME_CORE)) {
        return false;
    }

    siginfo_t ntSiginfo;
    if (tid == targetTid_) {
        ntSiginfo = coredumpKeyThread_.ntSiginfo;
    } else {
        ntSiginfo = CoredumpGetSiginfo(tid);
    }

    if (!bw_.Write(&ntSiginfo, sizeof(ntSiginfo))) {
        DFXLOGE("Write ntSiginfo fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool ThreadNoteWriter::ArmTaggedAddrCtrlWrite()
{
    if (!NoteUtil().NoteWrite(bw_, NT_ARM_TAGGED_ADDR_CTRL, sizeof(long), NOTE_NAME_LINUX)) {
        return false;
    }

    (void)memset_s(bw_.GetCurrent(), sizeof(long), 0, sizeof(long));
    bw_.Advance(sizeof(long));
    return true;
}

bool ThreadNoteWriter::GetPrStatus(prstatus_t &ntPrstatus, pid_t tid)
{
    if (tid == targetTid_ && coredumpKeyThread_.prStatusValid) {
        ntPrstatus.pr_cursig = coredumpKeyThread_.ntPrstatus.pr_info.si_signo;
    } else {
        if (GetSiginfoCommon(ntPrstatus.pr_info, tid)) {
            ntPrstatus.pr_cursig = ntPrstatus.pr_info.si_signo;
        }
    }

    std::string filePath = "/proc/" + std::to_string(tid) + "/status";
    std::ifstream file(filePath);

    if (!file.is_open()) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (StartsWith(line, PPID) == 0) {
            if (sscanf_s(line.c_str() + strlen(PPID), "%d", &ntPrstatus.pr_ppid) != 1) {
                continue;
            }
        } else if (StartsWith(line, SIGPND)) {
            if (sscanf_s(line.c_str() + strlen(SIGPND), "%lx", &ntPrstatus.pr_sigpend) != 1) {
                continue;
            }
        } else if (StartsWith(line, SIGBLK)) {
            if (sscanf_s(line.c_str() + strlen(SIGBLK), "%lx", &ntPrstatus.pr_sighold) != 1) {
                continue;
            }
        }
    }

    ntPrstatus.pr_pid = tid;
    ntPrstatus.pr_pgrp = getpgrp();
    ntPrstatus.pr_sid = getsid(ntPrstatus.pr_pid);
    if (!GetRusage(ntPrstatus)) {
        DFXLOGE("failed to get rusage, tid:%{public}d, errno:%{public}d", tid, errno);
        return false;
    }
    return true;
}


bool ThreadNoteWriter::GetRusage(prstatus_t &ntPrstatus)
{
    struct rusage selfUsage;
    getrusage(RUSAGE_SELF, &selfUsage);
    if (memcpy_s(&ntPrstatus.pr_utime, sizeof(ntPrstatus.pr_utime),
        &selfUsage.ru_utime, sizeof(selfUsage.ru_utime)) != 0) {
        return false;
    }
    if (memcpy_s(&ntPrstatus.pr_stime, sizeof(ntPrstatus.pr_stime),
        &selfUsage.ru_stime, sizeof(selfUsage.ru_stime)) != 0) {
        return false;
    }
    struct rusage childrenUsage;
    getrusage(RUSAGE_CHILDREN, &childrenUsage);
    if (memcpy_s(&ntPrstatus.pr_cutime, sizeof(ntPrstatus.pr_cutime),
        &childrenUsage.ru_utime, sizeof(childrenUsage.ru_utime)) != 0) {
        return false;
    }
    if (memcpy_s(&ntPrstatus.pr_cstime, sizeof(ntPrstatus.pr_cstime),
        &childrenUsage.ru_stime, sizeof(childrenUsage.ru_stime)) != 0) {
        return false;
    }
    return true;
}

bool ThreadNoteWriter::GetPrReg(prstatus_t &ntPrstatus, pid_t tid)
{
    if (tid == targetTid_) {
        if (!coredumpKeyThread_.keyRegs_) {
            return false;
        }
        if (memcpy_s(&(ntPrstatus.pr_reg), sizeof(ntPrstatus.pr_reg), coredumpKeyThread_.keyRegs_->RawData(),
            sizeof(ntPrstatus.pr_reg)) != 0) {
            DFXLOGE("Failed to memcpy regs data, errno = %{public}d", errno);
            return false;
        }
        ntPrstatus.pr_fpvalid = coredumpKeyThread_.fpRegValid;
    } else {
        struct iovec iov;
        (void)memset_s(&iov, sizeof(iov), 0, sizeof(iov));
        iov.iov_base = &(ntPrstatus.pr_reg);
        iov.iov_len = sizeof(ntPrstatus.pr_reg);
        if (ptrace(PTRACE_GETREGSET, tid, NT_PRSTATUS, &iov) == -1) {
            DFXLOGE("ptrace failed NT_PRSTATUS, tid:%{public}d, errno:%{public}d", tid, errno);
            return false;
        }
        struct user_fpsimd_struct ntFpregset;
        if (GetRegset(tid, NT_FPREGSET, ntFpregset)) {
            ntPrstatus.pr_fpvalid = 1;
        } else {
            ntPrstatus.pr_fpvalid = 0;
        }
    }
    return true;
}

UserPacMask CoredumpGetPackMask(pid_t tid)
{
    UserPacMask ntUserPacMask;
    GetRegset(tid, NT_ARM_PAC_MASK, ntUserPacMask);
    return ntUserPacMask;
}

int CoredumpGetFpRegset(pid_t tid, struct user_fpsimd_struct& ntFpregset)
{
    return GetRegset(tid, NT_FPREGSET, ntFpregset) ? 1 : 0;
}

siginfo_t CoredumpGetSiginfo(pid_t tid)
{
    siginfo_t ntSiginfo;
    GetSiginfoCommon(ntSiginfo, tid);
    return ntSiginfo;
}

int CoredumpGetPrstatus(pid_t tid, prstatus_t& ntPrstatus)
{
    if (GetSiginfoCommon(ntPrstatus.pr_info, tid)) {
        return 1;
    }
    return 0;
}

void NoteSegmentWriter::SetKeyThreadData(CoredumpThread coredumpKeyThread)
{
    coredumpKeyThread_ = coredumpKeyThread;
}

bool MultiThreadNoteWriter::Write()
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/task";
    DIR *dir = opendir(filePath.c_str());
    if (!dir) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
            pid_t tid = strtol(entry->d_name, nullptr, 10);
            auto tnw  = ThreadNoteWriter(tid, targetTid_, bw_);
            tnw.SetKeyThreadData(coredumpKeyThread_);
            tnw.Write();
        }
    }
    closedir(dir);
    return true;
}

bool PrpsinfoWriter::Write()
{
    NoteUtil().NoteWrite(bw_, NT_PRPSINFO, sizeof(prpsinfo_t), NOTE_NAME_CORE);
    prpsinfo_t ntPrpsinfo;
    FillPrpsinfo(ntPrpsinfo);
    if (!bw_.Write(&ntPrpsinfo, sizeof(ntPrpsinfo))) {
        DFXLOGE("Write ntPrpsinfo fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

void PrpsinfoWriter::FillPrpsinfo(prpsinfo_t &ntPrpsinfo)
{
    ReadProcessStat(ntPrpsinfo);
    ReadProcessStatus(ntPrpsinfo);
    ReadProcessComm(ntPrpsinfo);
    ReadProcessCmdline(ntPrpsinfo);
}

bool NoteUtil::NoteWrite(CoredumpBufferWriter& bw, uint32_t noteType, size_t descSize, const char* noteName)
{
    Elf64_Nhdr note;
    note.n_namesz = strlen(noteName) + 1;
    note.n_descsz = descSize;
    note.n_type = noteType;
    if (!bw.Write(&note, sizeof(note))) {
        DFXLOGE("Write note fail, errno:%{public}d", errno);
        return false;
    }
    constexpr size_t nameSize = 8;
    if (memcpy_s(bw.GetCurrent(), nameSize, noteName, nameSize) != EOK) {
        DFXLOGE("memcpy fail, errno:%{public}d", errno);
        return false;
    }
    bw.Advance(nameSize);
    return true;
}

bool PrpsinfoWriter::ReadProcessStat(prpsinfo_t &ntPrpsinfo)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/stat";
    std::string line;
    LoadStringFromFile(filePath, line);
    if (line.empty()) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }

    std::istringstream iss(line);

    int pid;
    std::string comm;
    char state;
    unsigned long prFlag;
    int dummyInt;

    iss >> pid >> comm >> state;
    for (int i = 0; i < 5; ++i) { // 5 : jump unused stat filed
        iss >> dummyInt;
    }
    iss >> prFlag;

    ntPrpsinfo.pr_state = state;
    ntPrpsinfo.pr_sname = state;
    ntPrpsinfo.pr_zomb = (state == 'Z') ? 1 : 0;
    ntPrpsinfo.pr_flag = prFlag;
    return true;
}

bool PrpsinfoWriter::ReadProcessStatus(prpsinfo_t &ntPrpsinfo)
{
    ntPrpsinfo.pr_nice = getpriority(PRIO_PROCESS, pid_);
    unsigned int prUid = 0;
    unsigned int prGid = 0;
    int prPpid = 0;

    std::string filePath = "/proc/" + std::to_string(pid_) + "/status";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (StartsWith(line, UID)) {
            if (sscanf_s(line.c_str() + strlen(UID), "%u", &prUid) != 1) {
                continue;
            }
        } else if (StartsWith(line, GID)) {
            if (sscanf_s(line.c_str() + strlen(GID), "%u", &prGid) != 1) {
                continue;
            }
        } else if (StartsWith(line, PPID)) {
            if (sscanf_s(line.c_str() + strlen(PPID), "%d", &prPpid) != 1) {
                continue;
            }
        }
    }

    ntPrpsinfo.pr_uid = prUid;
    ntPrpsinfo.pr_gid = prGid;
    ntPrpsinfo.pr_pid = pid_;
    ntPrpsinfo.pr_ppid = prPpid;
    ntPrpsinfo.pr_pgrp = getpgrp();
    ntPrpsinfo.pr_sid = getsid(ntPrpsinfo.pr_pid);
    return true;
}

bool PrpsinfoWriter::ReadProcessComm(prpsinfo_t &ntPrpsinfo)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/comm";
    std::string line;
    LoadStringFromFile(filePath, line);
    if (line.empty()) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }

    auto ret = strncpy_s(ntPrpsinfo.pr_fname, sizeof(ntPrpsinfo.pr_fname), line.c_str(),
        sizeof(ntPrpsinfo.pr_fname) - 1);
    ntPrpsinfo.pr_fname[sizeof(ntPrpsinfo.pr_fname) - 1] = '\0';
    if (ret != 0) {
        DFXLOGE("read comm fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool PrpsinfoWriter::ReadProcessCmdline(prpsinfo_t &ntPrpsinfo)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/cmdline";
    std::string cmdLine;
    LoadStringFromFile(filePath, cmdLine);
    if (cmdLine.empty()) {
        return false;
    }

    auto ret = strncpy_s(ntPrpsinfo.pr_psargs, sizeof(ntPrpsinfo.pr_psargs),
        cmdLine.c_str(), sizeof(ntPrpsinfo.pr_psargs) - 1);
    ntPrpsinfo.pr_psargs[sizeof(ntPrpsinfo.pr_psargs) - 1] = '\0';
    if (ret != 0) {
        DFXLOGE("strncpy_s fail, err:%{public}d", ret);
        return false;
    }
    return true;
}

void ThreadNoteWriter::SetKeyThreadData(CoredumpThread coreDumpKeyThreadData)
{
    coredumpKeyThread_ = coreDumpKeyThreadData;
}

bool AuxvWriter::Write()
{
    Elf64_Nhdr *note = reinterpret_cast<Elf64_Nhdr *>(bw_.GetCurrent());
    note->n_namesz = sizeof("CORE");
    note->n_type = NT_AUXV;

    bw_.Advance(sizeof(Elf64_Nhdr));
    char noteName[NOTE_NAME_SIZE] = "CORE";
    if (!bw_.Write(noteName, sizeof(noteName))) {
        DFXLOGE("Wrtie noteName fail, errno:%{public}d", errno);
        return false;
    }

    if (!ReadProcessAuxv(note)) {
        DFXLOGE("Read targetPid Process Auxv fail");
    }
    return true;
}

bool AuxvWriter::ReadProcessAuxv(Elf64_Nhdr *note)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/auxv";
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        note->n_descsz = 0;
        DFXLOGE("open %s fail, errno:%d", filePath.c_str(), errno);
        return false;
    }

    Elf64_auxv_t ntAuxv;
    note->n_descsz = 0;

    while (file.read(reinterpret_cast<char*>(&ntAuxv), sizeof(Elf64_auxv_t))) {
        note->n_descsz += sizeof(ntAuxv);
        if (!bw_.Write(&ntAuxv, sizeof(ntAuxv))) {
            DFXLOGE("Write ntAuxv fail, errno:%d", errno);
            return false;
        }
        if (ntAuxv.a_type == AT_NULL) {
            break;
        }
    }

    return true;
}

bool FileRegionWriter::Write()
{
    Elf64_Nhdr *note = reinterpret_cast<Elf64_Nhdr *>(bw_.GetCurrent());
    note->n_namesz = sizeof("CORE");
    note->n_type = NT_FILE;

    bw_.Advance(sizeof(Elf64_Nhdr));
    char noteName[NOTE_NAME_SIZE] = "CORE";
    if (!bw_.Write(noteName, sizeof(noteName))) {
        DFXLOGE("Wrtie notename fail, errno:%{public}d", errno);
        return false;
    }

    char *startPointer = bw_.GetCurrent();
    FileHeader *ntFileHd = reinterpret_cast<FileHeader *>(bw_.GetCurrent());
    ntFileHd->pageSize = 0x01;
    bw_.Advance(sizeof(FileHeader));

    Elf64_Half lineNumber = 1;
    WriteAddrRelated();
    WriteFilePath(lineNumber);

    note->n_descsz = reinterpret_cast<uintptr_t>(bw_.GetCurrent()) - reinterpret_cast<uintptr_t>(startPointer);
    ntFileHd->count = lineNumber - 1;

    constexpr uint8_t fileNoteAlignment = 4;
    uint8_t remain = note->n_descsz % fileNoteAlignment;
    for (uint8_t i = 0; i < (fileNoteAlignment - remain); i++) {
        if (remain == 0) {
            break;
        }
        *bw_.GetCurrent() = 0;
        bw_.Advance(1);
    }
    return true;
}

bool FileRegionWriter::WriteAddrRelated()
{
    for (const auto& region : maps_) {
        if (region.pathName[0] != '/' || (region.priority[0] != 'r')) {
            continue;
        }
        FileMap ntFile;
        ntFile.start = region.startHex;
        ntFile.end = region.endHex;
        ntFile.offset = region.offsetHex;
        if (!bw_.Write(&ntFile, sizeof(ntFile))) {
            DFXLOGE("Wrtie ntFile fail, errno:%{public}d", errno);
            return false;
        }
    }
    return true;
}

bool FileRegionWriter::WriteFilePath(Elf64_Half &lineNumber)
{
    for (const auto& region : maps_) {
        if (region.pathName[0] != '/' || (region.priority[0] != 'r')) {
            continue;
        }
        if (!bw_.Write(region.pathName, strlen(region.pathName))) {
            DFXLOGE("Wrtie pathName fail, errno:%{public}d", errno);
            return false;
        }
        *bw_.GetCurrent() = '\0';
        bw_.Advance(1);
        lineNumber += 1;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
