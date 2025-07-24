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
#if defined(__aarch64__)
#include "dfx_coredump_writer.h"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
static const char NOTE_NAME_CORE[8] = "CORE";
static const char NOTE_NAME_LINUX[8] = "LINUX";
const char *const UID = "Uid:";
const char *const GID = "Gid:";
const char *const PPID = "PPid:";
const char *const SIGPND = "SigPnd:";
const char *const SIGBLK = "SigBlk:";

char* ProgramSegmentHeaderWriter::Write()
{
    Elf64_Phdr ptNote;
    PtNoteFill(ptNote);

    if (!CopyAndAdvance(&ptNote, sizeof(ptNote))) {
        DFXLOGE("Write ptNote fail, errno:%{public}d", errno);
        return currentPointer_;
    }

    Elf64_Phdr ptLoad;
    Elf64_Half lineNumber = 1;

    for (const auto &region : maps_) {
        if (!strcmp(region.pathName, "[vvar]") || (region.priority[0] != 'r')) {
            continue;
        }
        PtLoadFill(ptLoad, region);
        if (!CopyAndAdvance(&ptLoad, sizeof(ptLoad))) {
            DFXLOGE("Write ptLoad fail, errno:%{public}d", errno);
            return currentPointer_;
        }
        lineNumber++;
    }

    char *ptNoteAddr = mappedMemory_ + sizeof(Elf64_Ehdr);
    char *ptLoadAddr = ptNoteAddr + sizeof(Elf64_Phdr);

    char *pOffsetAddr = ptLoadAddr + offsetof(Elf64_Phdr, p_offset);
    Elf64_Off *pOffset = reinterpret_cast<Elf64_Off *>(pOffsetAddr);
    *pOffset = sizeof(Elf64_Ehdr) + (lineNumber * sizeof(Elf64_Phdr));

    char *pFileszAddrLast = ptLoadAddr + offsetof(Elf64_Phdr, p_filesz);
    Elf64_Xword *pFileszLast = reinterpret_cast<Elf64_Xword *>(pFileszAddrLast);

    Elf64_Off pOffsetValueLast = *pOffset;

    for (Elf64_Half i = 0; i < (lineNumber - 2); i++) { // 2
        pOffsetAddr += sizeof(Elf64_Phdr);
        pOffset = reinterpret_cast<Elf64_Off *>(pOffsetAddr);

        *pOffset = pOffsetValueLast + *pFileszLast;

        pOffsetValueLast = *pOffset;
        pFileszAddrLast += sizeof(Elf64_Phdr);
        pFileszLast = reinterpret_cast<Elf64_Xword *>(pFileszAddrLast);
    }

    pOffsetAddr = ptNoteAddr + offsetof(Elf64_Phdr, p_offset);
    pOffset = reinterpret_cast<Elf64_Off *>(pOffsetAddr);

    *pOffset = pOffsetValueLast + *pFileszLast;

    ePhnum_ = lineNumber;
    return currentPointer_;
}

void ProgramSegmentHeaderWriter::PtLoadFill(Elf64_Phdr &ph, const DumpMemoryRegions &region)
{
    Elf64_Word pFlags = 0x00;
    if (region.priority[0] == 'r' || region.priority[0] == '-') {
        pFlags |= PF_R;
    }
    if (region.priority[1] == 'w') {
        pFlags |= PF_W;
    }
    if (region.priority[2] == 'x') { // 2
        pFlags |= PF_X;
    }
    ProgramSegmentHeaderFill(ph, PT_LOAD, pFlags, region);
}

void ProgramSegmentHeaderWriter::PtNoteFill(Elf64_Phdr &ph)
{
    struct DumpMemoryRegions region;
    Elf64_Word pFlags = PF_R;
    region.startHex = 0x00;
    region.endHex = 0x00;
    region.memorySizeHex = 0x00;
    ProgramSegmentHeaderFill(ph, PT_NOTE, pFlags, region);
}

void ProgramSegmentHeaderWriter::ProgramSegmentHeaderFill(Elf64_Phdr &ph, Elf64_Word pType,
    Elf64_Word pFlags, const DumpMemoryRegions &region)
{
    ph.p_type = pType;
    ph.p_flags = pFlags;
    ph.p_offset = sizeof(Elf64_Ehdr);
    ph.p_vaddr = region.startHex;
    ph.p_paddr = 0x00;
    ph.p_filesz = region.memorySizeHex;
    ph.p_memsz = ph.p_filesz;
    ph.p_align = 0x1;
}

char* LoadSegmentWriter::Write()
{
    char *ptLoadAddr = mappedMemory_ + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    Elf64_Phdr *ptLoad = reinterpret_cast<Elf64_Phdr *>(ptLoadAddr);
    if (ptLoad == nullptr) {
        return currentPointer_;
    }
    for (Elf64_Half i = 0; i < (ePhnum_ - 1); i++) {
        ReadProcessVmmem(*ptLoad);
        ptLoad += 1;
    }
    return currentPointer_;
}

void LoadSegmentWriter::ReadProcessVmmem(Elf64_Phdr &ptLoad)
{
    struct iovec local[1];
    struct iovec remote[1];

    local[0].iov_base = currentPointer_;
    local[0].iov_len = ptLoad.p_memsz;

    remote[0].iov_base = reinterpret_cast<void *>(ptLoad.p_vaddr);
    remote[0].iov_len = ptLoad.p_memsz;
    ssize_t nread = process_vm_readv(pid_, local, 1, remote, 1, 0);
    if (nread == -1) {
        (void)memset_s(currentPointer_, ptLoad.p_memsz, -1, ptLoad.p_memsz);
    }
    ptLoad.p_offset = reinterpret_cast<uintptr_t>(currentPointer_) - reinterpret_cast<uintptr_t>(mappedMemory_);
    currentPointer_ += ptLoad.p_memsz;
}

char* NoteSegmentWriter::Write()
{
    char *noteStart = currentPointer_;
    PrpsinfoWrite();
    MultiThreadNoteWrite();
    AuxvWrite();
    FileWrite();

    Elf64_Phdr *ptNote = reinterpret_cast<Elf64_Phdr *>(mappedMemory_ + sizeof(Elf64_Ehdr));

    ptNote->p_filesz = reinterpret_cast<uintptr_t>(currentPointer_) - reinterpret_cast<uintptr_t>(noteStart);

    ptNote->p_offset = reinterpret_cast<uintptr_t>(noteStart) - reinterpret_cast<uintptr_t>(mappedMemory_);
    return currentPointer_;
}

bool NoteSegmentWriter::PrpsinfoWrite()
{
    NoteWrite(NT_PRPSINFO, sizeof(prpsinfo_t), NOTE_NAME_CORE);
    prpsinfo_t ntPrpsinfo;
    FillPrpsinfo(ntPrpsinfo);
    if (!CopyAndAdvance(&ntPrpsinfo, sizeof(ntPrpsinfo))) {
        DFXLOGE("Write ntPrpsinfo fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

void NoteSegmentWriter::FillPrpsinfo(prpsinfo_t &ntPrpsinfo)
{
    if (!ReadProcessStat(ntPrpsinfo)) {
        DFXLOGE("Read targetPid process stat fail!");
    }
    if (!ReadProcessStatus(ntPrpsinfo)) {
        DFXLOGE("Read targetPid process status fail!");
    }
    if (!ReadProcessComm(ntPrpsinfo)) {
        DFXLOGE("Read targetPid process comm fail!");
    }
    if (!ReadProcessCmdline(ntPrpsinfo)) {
        DFXLOGE("Read targetPid process cmdline fail!");
    }
}

bool NoteSegmentWriter::NoteWrite(uint32_t noteType, size_t descSize, const char* noteName)
{
    Elf64_Nhdr note;
    note.n_namesz = strlen(noteName) + 1;
    note.n_descsz = descSize;
    note.n_type = noteType;
    if (!CopyAndAdvance(&note, sizeof(note))) {
        DFXLOGE("Write note fail, errno:%{public}d", errno);
        return false;
    }
    constexpr size_t nameSize = 8; // 8
    if (memcpy_s(currentPointer_, nameSize, noteName, nameSize) != EOK) {
        DFXLOGE("memcpy fail, errno:%{public}d", errno);
        return false;
    }
    currentPointer_ += nameSize;
    return true;
}

bool NoteSegmentWriter::ReadProcessStat(prpsinfo_t &ntPrpsinfo)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/stat";
    std::ifstream statFile(filePath);
    if (!statFile.is_open()) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }

    std::string line;
    if (!std::getline(statFile, line)) {
        DFXLOGE("read %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }

    std::istringstream iss(line);

    int pid;
    std::string comm;
    char state;
    unsigned long prFlag;
    int dummyInt;

    iss >> pid >> comm >> state;

    for (int i = 0; i < 5; ++i) { // 5
        iss >> dummyInt;
    }

    iss >> prFlag;

    ntPrpsinfo.pr_state = state;
    ntPrpsinfo.pr_sname = state;
    ntPrpsinfo.pr_zomb = (state == 'Z') ? 1 : 0;
    ntPrpsinfo.pr_flag = prFlag;
    statFile.close();
    return true;
}

bool NoteSegmentWriter::ReadProcessStatus(prpsinfo_t &ntPrpsinfo)
{
    ntPrpsinfo.pr_nice = getpriority(PRIO_PROCESS, pid_);
    unsigned int prUid = 0;
    unsigned int prGid = 0;
    int prPpid = 0;
    char buffer[1024];

    std::string filePath = "/proc/" + std::to_string(pid_) + "/status";
    FILE *file = fopen(filePath.c_str(), "r");
    if (!file) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }

    while (fgets(buffer, sizeof(buffer), file) != nullptr) {
        if (strncmp(buffer, UID, strlen(UID)) == 0) {
            if (sscanf_s(buffer + strlen(UID), "%u", &prUid) != 1) {
                continue;
            }
        } else if (strncmp(buffer, GID, strlen(GID)) == 0) {
            if (sscanf_s(buffer + strlen(GID), "%u", &prGid) != 1) {
                continue;
            }
        } else if (strncmp(buffer, PPID, strlen(PPID)) == 0) {
            if (sscanf_s(buffer + strlen(PPID), "%d", &prPpid) != 1) {
                continue;
            }
        }
    }
    (void)fclose(file);
    ntPrpsinfo.pr_uid = prUid;
    ntPrpsinfo.pr_gid = prGid;
    ntPrpsinfo.pr_pid = pid_;
    ntPrpsinfo.pr_ppid = prPpid;
    ntPrpsinfo.pr_pgrp = getpgrp();
    ntPrpsinfo.pr_sid = getsid(ntPrpsinfo.pr_pid);
    return true;
}

bool NoteSegmentWriter::ReadProcessComm(prpsinfo_t &ntPrpsinfo)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/comm";
    FILE *file = fopen(filePath.c_str(), "r");
    if (!file) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    if (fgets(ntPrpsinfo.pr_fname, sizeof(ntPrpsinfo.pr_fname), file) == nullptr) {
        DFXLOGE("read comm fail, errno:%{public}d", errno);
        (void)fclose(file);
        return false;
    }
    (void)fclose(file);
    return true;
}

bool NoteSegmentWriter::ReadProcessCmdline(prpsinfo_t &ntPrpsinfo)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/cmdline";
    FILE *file = fopen(filePath.c_str(), "r");
    if (!file) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    char cmdline[256];
    size_t len = fread(cmdline, 1, sizeof(cmdline) - 1, file);
    (void)fclose(file);

    if (len <= 0) {
        DFXLOGE("read %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    cmdline[len] = '\0';

    (void)memset_s(&ntPrpsinfo.pr_psargs, sizeof(ntPrpsinfo.pr_psargs), 0, sizeof(ntPrpsinfo.pr_psargs));
    auto ret = strncpy_s(ntPrpsinfo.pr_psargs, sizeof(ntPrpsinfo.pr_psargs),
        cmdline, sizeof(ntPrpsinfo.pr_psargs) - 1);
    if (ret != 0) {
        DFXLOGE("strncpy_s fail, err:%{public}d", ret);
        return false;
    }
    return true;
}

bool NoteSegmentWriter::MultiThreadNoteWrite()
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
            ThreadNoteWrite(tid);
        }
    }
    closedir(dir);
    return true;
}

bool NoteSegmentWriter::ThreadNoteWrite(pid_t tid)
{
    if (tid == targetTid_) {
        ptrace(PTRACE_INTERRUPT, tid, 0, 0);
        if (waitpid(tid, nullptr, 0) < 0) {
            DFXLOGE("Failed to waitpid tid(%{public}d), errno=%{public}d", tid, errno);
            return false;
        }
    }
    if (!PrstatusWrite(tid)) {
        DFXLOGE("Failed to write prstatus in (%{public}d)", tid);
    }
    if (!ArmPacMaskWrite(tid)) {
        DFXLOGE("Failed to write arm_pac_mask in (%{public}d)", tid);
    }
    if (!FpregsetWrite(tid)) {
        DFXLOGE("Failed to write fp reg in (%{public}d)", tid);
    }
    if (!SiginfoWrite(tid)) {
        DFXLOGE("Failed to write siginfo in (%{public}d)", tid);
    }
    if (!ArmTaggedAddrCtrlWrite()) {
        DFXLOGE("Failed to write arm tagged");
    }
    if (tid == targetTid_) {
        ptrace(PTRACE_CONT, tid, 0, 0);
    }
    return true;
}

bool NoteSegmentWriter::ArmPacMaskWrite(pid_t tid)
{
    if (!NoteWrite(NT_ARM_PAC_MASK, sizeof(UserPacMask), NOTE_NAME_LINUX)) {
        return false;
    }

    struct iovec iov;
    UserPacMask ntUserPacMask;
    iov.iov_base = &ntUserPacMask;
    iov.iov_len = sizeof(ntUserPacMask);
    if (ptrace(PTRACE_GETREGSET, tid, NT_ARM_PAC_MASK, &iov) == -1) {
        DFXLOGE("ptrace failed NT_ARM_PAC_MASK, tid:%{public}d,, errno:%{public}d", tid, errno);
        return false;
    }
    if (!CopyAndAdvance(&ntUserPacMask, sizeof(ntUserPacMask))) {
        DFXLOGE("Write ntUserPacMask fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool NoteSegmentWriter::PrstatusWrite(pid_t tid)
{
    NoteWrite(NT_PRSTATUS, sizeof(prstatus_t), NOTE_NAME_CORE);
    prstatus_t ntPrstatus;
    if (!GetPrStatus(ntPrstatus, tid)) {
        DFXLOGE("Fail to get pr status in (%{public}d)", tid);
    }
    if (!GetPrReg(ntPrstatus, tid)) {
        DFXLOGE("Fail to get pr reg in (%{public}d)", tid);
    }
    if (!CopyAndAdvance(&ntPrstatus, sizeof(ntPrstatus))) {
        DFXLOGE("Write ntPrstatus fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool NoteSegmentWriter::FpregsetWrite(pid_t tid)
{
    if (!NoteWrite(NT_FPREGSET, sizeof(struct user_fpsimd_struct), NOTE_NAME_CORE)) {
        return false;
    }
    struct iovec iov;
    struct user_fpsimd_struct ntFpregset;
    iov.iov_base = &ntFpregset;
    iov.iov_len = sizeof(ntFpregset);

    if (ptrace(PTRACE_GETREGSET, tid, NT_FPREGSET, &iov) == -1) {
        DFXLOGE("ptrace failed NT_FPREGSET, tid:%{public}d,, errno:%{public}d", tid, errno);
        return false;
    }
    if (!CopyAndAdvance(&ntFpregset, sizeof(ntFpregset))) {
        DFXLOGE("Write ntFpregset fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool NoteSegmentWriter::SiginfoWrite(pid_t tid)
{
    if (!NoteWrite(NT_SIGINFO, sizeof(siginfo_t), NOTE_NAME_CORE)) {
        return false;
    }

    siginfo_t ntSiginfo;

    if (ptrace(PTRACE_GETSIGINFO, tid, nullptr, &ntSiginfo) == -1) {
        DFXLOGE("ptrace failed PTRACE_GETSIGINFO, tid:%{public}d,, errno:%{public}d", tid, errno);
    }
    if (!CopyAndAdvance(&ntSiginfo, sizeof(ntSiginfo))) {
        DFXLOGE("Write ntSiginfo fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool NoteSegmentWriter::ArmTaggedAddrCtrlWrite()
{
    if (!NoteWrite(0x409, sizeof(long), NOTE_NAME_LINUX)) {
        return false;
    }

    (void)memset_s(currentPointer_, sizeof(long), 0, sizeof(long));
    currentPointer_ += sizeof(long);
    return true;
}

bool NoteSegmentWriter::GetPrStatus(prstatus_t &ntPrstatus, pid_t tid)
{
    if (ptrace(PTRACE_GETSIGINFO, tid, NULL, &(ntPrstatus.pr_info)) == 0) {
        ntPrstatus.pr_cursig = ntPrstatus.pr_info.si_signo;
    } else {
        DFXLOGE("ptrace failed PTRACE_GETSIGINFO, tid:%{public}d, errno:%{public}d", tid, errno);
    }
    char buffer[1024];
    std::string filePath = "/proc/" + std::to_string(tid) + "/status";
    FILE *file = fopen(filePath.c_str(), "r");
    if (!file) {
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    while (fgets(buffer, sizeof(buffer), file) != nullptr) {
        if (strncmp(buffer, PPID, strlen(PPID)) == 0) {
            if (sscanf_s(buffer + strlen(PPID), "%d", &ntPrstatus.pr_ppid) != 1) {
                continue;
            }
        } else if (strncmp(buffer, SIGPND, strlen(SIGPND)) == 0) {
            if (sscanf_s(buffer + strlen(SIGPND), "%lx", &ntPrstatus.pr_sigpend) != 1) {
                continue;
            }
        } else if (strncmp(buffer, SIGBLK, strlen(SIGBLK)) == 0) {
            if (sscanf_s(buffer + strlen(SIGBLK), "%lx", &ntPrstatus.pr_sighold) != 1) {
                continue;
            }
        }
    }

    (void)fclose(file);
    ntPrstatus.pr_pid = tid;
    ntPrstatus.pr_pgrp = getpgrp();
    ntPrstatus.pr_sid = getsid(ntPrstatus.pr_pid);
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    if (memcpy_s(&ntPrstatus.pr_utime, sizeof(ntPrstatus.pr_utime), &usage.ru_utime, sizeof(usage.ru_utime)) != 0) {
        return false;
    }
    if (memcpy_s(&ntPrstatus.pr_stime, sizeof(ntPrstatus.pr_stime), &usage.ru_stime, sizeof(usage.ru_stime)) != 0) {
        return false;
    }
    getrusage(RUSAGE_CHILDREN, &usage);
    if (memcpy_s(&ntPrstatus.pr_cutime, sizeof(ntPrstatus.pr_cutime), &usage.ru_utime, sizeof(usage.ru_utime)) != 0) {
        return false;
    }
    if (memcpy_s(&ntPrstatus.pr_cstime, sizeof(ntPrstatus.pr_cstime), &usage.ru_stime, sizeof(usage.ru_stime)) != 0) {
        return false;
    }
    return true;
}

bool NoteSegmentWriter::GetPrReg(prstatus_t &ntPrstatus, pid_t tid)
{
    if (tid == targetTid_) {
        if (memcpy_s(&(ntPrstatus.pr_reg), sizeof(ntPrstatus.pr_reg), keyRegs_->RawData(),
            sizeof(ntPrstatus.pr_reg)) != 0) {
            DFXLOGE("Failed to memcpy regs data, errno = %{public}d", errno);
            return false;
        }
    } else {
        struct iovec iov;
        (void)memset_s(&iov, sizeof(iov), 0, sizeof(iov));
        iov.iov_base = &(ntPrstatus.pr_reg);
        iov.iov_len = sizeof(ntPrstatus.pr_reg);
        if (ptrace(PTRACE_GETREGSET, tid, NT_PRSTATUS, &iov) == -1) {
            DFXLOGE("ptrace failed NT_PRSTATUS, tid:%{public}d, errno:%{public}d", tid, errno);
            return false;
        }
    }
    struct iovec iovFp;
    struct user_fpsimd_struct ntFpregset;
    iovFp.iov_base = &ntFpregset;
    iovFp.iov_len = sizeof(ntFpregset);

    if (ptrace(PTRACE_GETREGSET, tid, NT_FPREGSET, &iovFp) == -1) {
        DFXLOGE("ptrace failed NT_FPREGSET, tid:%{public}d, errno:%{public}d", tid, errno);
        ntPrstatus.pr_fpvalid = 0;
    } else {
        ntPrstatus.pr_fpvalid = 1;
    }
    return true;
}

bool NoteSegmentWriter::AuxvWrite()
{
    Elf64_Nhdr *note = reinterpret_cast<Elf64_Nhdr *>(currentPointer_);
    note->n_namesz = 0x05;
    note->n_type = NT_AUXV;

    currentPointer_ += sizeof(Elf64_Nhdr);
    char noteName[8] = "CORE";
    if (!CopyAndAdvance(noteName, sizeof(noteName))) {
        DFXLOGE("Wrtie noteName fail, errno:%{public}d", errno);
        return false;
    }

    if (!ReadProcessAuxv(note)) {
        DFXLOGE("Read targetPid Process Auxv fail");
    }
    return true;
}

bool NoteSegmentWriter::ReadProcessAuxv(Elf64_Nhdr *note)
{
    std::string filePath = "/proc/" + std::to_string(pid_) + "/auxv";
    FILE *file = fopen(filePath.c_str(), "r");
    if (!file) {
        note->n_descsz = 0;
        DFXLOGE("open %{public}s fail, errno:%{public}d", filePath.c_str(), errno);
        return false;
    }
    Elf64_auxv_t ntAuxv;
    note->n_descsz = 0;
    while (fread(&ntAuxv, sizeof(Elf64_auxv_t), 1, file) == 1) {
        note->n_descsz += sizeof(ntAuxv);
        if (!CopyAndAdvance(&ntAuxv, sizeof(ntAuxv))) {
            DFXLOGE("Wrtie ntAuxv fail, errno:%{public}d", errno);
            (void)fclose(file);
            return false;
        }
        if (ntAuxv.a_type == AT_NULL) {
            break;
        }
    }
    (void)fclose(file);
    return true;
}

bool NoteSegmentWriter::FileWrite()
{
    Elf64_Nhdr *note = reinterpret_cast<Elf64_Nhdr *>(currentPointer_);
    note->n_namesz = 0x05;
    note->n_type = NT_FILE;

    currentPointer_ += sizeof(Elf64_Nhdr);
    char noteName[8] = "CORE";
    if (!CopyAndAdvance(noteName, sizeof(noteName))) {
        DFXLOGE("Wrtie notename fail, errno:%{public}d", errno);
        return false;
    }

    char *startPointer = currentPointer_;
    FileHeader *ntFileHd = reinterpret_cast<FileHeader *>(currentPointer_);
    ntFileHd->pageSize = 0x01;
    currentPointer_ += sizeof(FileHeader);

    Elf64_Half lineNumber = 1;
    WriteAddrRelated();
    WriteFilePath(lineNumber);

    note->n_descsz = reinterpret_cast<uintptr_t>(currentPointer_) - reinterpret_cast<uintptr_t>(startPointer);
    ntFileHd->count = lineNumber - 1;

    uint8_t remain = note->n_descsz % 4; // 4
    for (uint8_t i = 0; i < (4 - remain); i++) { // 4
        if (remain == 0) {
            break;
        }
        *currentPointer_ = 0;
        currentPointer_ += 1;
    }
    return true;
}

bool NoteSegmentWriter::WriteAddrRelated()
{
    for (const auto& region : maps_) {
        if (region.pathName[0] != '/' || (region.priority[0] != 'r')) {
            continue;
        }
        FileMap ntFile;
        ntFile.start = region.startHex;
        ntFile.end = region.endHex;
        ntFile.offset = region.offsetHex;
        if (!CopyAndAdvance(&ntFile, sizeof(ntFile))) {
            DFXLOGE("Wrtie ntFile fail, errno:%{public}d", errno);
            return false;
        }
    }
    return true;
}

bool NoteSegmentWriter::WriteFilePath(Elf64_Half &lineNumber)
{
    for (const auto& region : maps_) {
        if (region.pathName[0] != '/' || (region.priority[0] != 'r')) {
            continue;
        }
        if (!CopyAndAdvance(region.pathName, strlen(region.pathName))) {
            DFXLOGE("Wrtie pathName fail, errno:%{public}d", errno);
            return false;
        }
        *currentPointer_ = '\0';
        currentPointer_ += 1;
        lineNumber += 1;
    }
    return true;
}

char* SectionHeaderTableWriter::Write()
{
    Elf64_Ehdr *elfHeader = reinterpret_cast<Elf64_Ehdr *>(mappedMemory_);
    Elf64_Phdr *programHeader = reinterpret_cast<Elf64_Phdr *>(mappedMemory_ + sizeof(Elf64_Ehdr));

    char *strTableAddr = currentPointer_;
    char strTable[22] = "..shstrtab.note0.load"; // 22
    (void)memset_s(currentPointer_, sizeof(strTable), 0, sizeof(strTable)); // 22

    strTable[0] = 0x00;
    strTable[10] = 0x00; // 10
    strTable[16] = 0x00; // 16
    if (!CopyAndAdvance(strTable, sizeof(strTable))) {
        DFXLOGE("Wrtie strTable fail, errno:%{public}d", errno);
        return currentPointer_;
    }

    Elf64_Off offset = Elf64_Off(currentPointer_ - mappedMemory_);
    uint8_t remain = offset % 8; // 8
    AdjustOffset(remain);
    elfHeader->e_shoff = static_cast<Elf64_Off>(currentPointer_ - mappedMemory_);
    (void)memset_s(currentPointer_, sizeof(Elf64_Shdr), 0, sizeof(Elf64_Shdr));
    currentPointer_ += sizeof(Elf64_Shdr);
    Elf64_Shdr *sectionHeader = reinterpret_cast<Elf64_Shdr *>(currentPointer_);
    sectionHeader->sh_name = 0x0B;
    SectionHeaderFill(sectionHeader, SHT_NOTE, SHF_ALLOC, programHeader);
    sectionHeader += 1;
    programHeader += 1;
    for (int i = 0; i < elfHeader->e_shnum - 3; i++) { // 3
        sectionHeader->sh_name = 0x11;
        Elf64_Xword shFlag = 0x00;
        if (programHeader->p_flags == PF_R) {
            shFlag = SHF_ALLOC;
        } else if (programHeader->p_flags == (PF_R | PF_W)) {
            shFlag = SHF_WRITE | SHF_ALLOC;
        } else if (programHeader->p_flags == (PF_R | PF_X)) {
            shFlag = SHF_ALLOC | SHF_EXECINSTR;
        }
        SectionHeaderFill(sectionHeader, SHT_PROGBITS, shFlag, programHeader);
        sectionHeader += 1;
        programHeader += 1;
    }
    sectionHeader->sh_name = 0x01;
    SectionHeaderFill(sectionHeader, SHT_STRTAB, 0x00, programHeader);
    sectionHeader->sh_addr = 0x00;

    sectionHeader->sh_offset = static_cast<Elf64_Off>(strTableAddr - mappedMemory_);
    sectionHeader->sh_size = 0x16;
    sectionHeader += 1;
    currentPointer_ = reinterpret_cast<char*>(sectionHeader);
    return currentPointer_;
}

void SectionHeaderTableWriter::AdjustOffset(uint8_t remain)
{
    if (remain > 8) { // 8
        return;
    }
    for (uint8_t i = 0; i < (8 - remain); i++) { // 8
        if (remain == 0) {
            break;
        }
        *currentPointer_ = 0;
        currentPointer_ += 1;
    }
}

void SectionHeaderTableWriter::SectionHeaderFill(Elf64_Shdr *sectionHeader, Elf64_Word shType,
    Elf64_Xword shFlag, Elf64_Phdr *programHeader)
{
    sectionHeader->sh_type = shType;
    sectionHeader->sh_flags = shFlag;
    sectionHeader->sh_addr = programHeader->p_vaddr;
    sectionHeader->sh_offset = programHeader->p_offset;
    sectionHeader->sh_size = programHeader->p_filesz;
    sectionHeader->sh_link = 0x00;
    sectionHeader->sh_info = 0x00;
    sectionHeader->sh_addralign = 0x1;
    sectionHeader->sh_entsize = 0x00;
}

} // namespace HiviewDFX
} // namespace OHOS
#endif