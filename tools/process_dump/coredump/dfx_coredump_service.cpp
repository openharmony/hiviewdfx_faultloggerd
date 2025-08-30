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
#include "dfx_coredump_service.h"

#include <fstream>
#include <fcntl.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "faultloggerd_client.h"
#include "securec.h"
#ifndef is_ohos_lite
#include "parameters.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
const char *const COREDUMP_DIR_PATH = "/data/storage/el2/base/files";
const char *const COREDUMP_HAP_WHITE_LIST = "const.dfx.coredump.hap_list";
const char *const HWASAN_COREDUMP_ENABLE = "faultloggerd.priv.hwasan_coredump.enabled";
char g_coredumpFilePath[256] = {0};
static const int ARG2 = 2;
static const int ARG16 = 16;
static const int ARG100 = 100;
static const int ARG1000 = 1000;

using namespace OHOS;

static std::string GetCoredumpWhiteList()
{
#ifndef is_ohos_lite
    std::string whiteList = OHOS::system::GetParameter(COREDUMP_HAP_WHITE_LIST, "");
    return whiteList;
#endif
    return "";
}

void HandleSigterm(int sig)
{
    unlink(g_coredumpFilePath);
    _exit(0);
}

int UnBlockSIGTERM()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, DUMPCATCHER_TIMEOUT);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    return 0;
}

int RegisterCancelCoredump(std::string logPath)
{
    auto ret = strncpy_s(g_coredumpFilePath, sizeof(g_coredumpFilePath), logPath.c_str(), logPath.length());
    if (ret != 0) {
        DFXLOGE("strncpy_s fail, err:%{public}d", ret);
        return -1;
    }

    UnBlockSIGTERM();
    if (signal(DUMPCATCHER_TIMEOUT, HandleSigterm) == SIG_ERR) {
        DFXLOGE("Failed to register handler for DUMPCATCHER_TIMEOUT");
        return -1;
    }
    return 0;
}
}

bool CoreDumpService::IsHwasanCoredumpEnabled()
{
#ifndef is_ohos_lite
    static bool isHwasanCoredumpEnabled = OHOS::system::GetParameter(HWASAN_COREDUMP_ENABLE, "false") == "true";
    return isHwasanCoredumpEnabled;
#else
    return false;
#endif
}

bool CoreDumpService::IsCoredumpSignal(const ProcessDumpRequest& request)
{
    return request.siginfo.si_signo == 42 && request.siginfo.si_code == 3; // 42 3
}

CoreDumpService::CoreDumpService(int32_t targetPid, int32_t targetTid, std::shared_ptr<DfxRegs> keyRegs)
{
    SetCoreDumpServiceData(targetPid, targetTid, keyRegs);
}

void CoreDumpService::SetCoreDumpServiceData(int32_t targetPid, int32_t targetTid, std::shared_ptr<DfxRegs> keyRegs)
{
    coreDumpThread_.targetPid = targetPid;
    coreDumpThread_.targetTid = targetTid;
    keyRegs_ = keyRegs;
}

CoreDumpService::~CoreDumpService()
{
    DeInit();
}

void CoreDumpService::SetVmPid(int32_t vmPid)
{
    coreDumpThread_.vmPid = vmPid;
}

void CoreDumpService::StartFirstStageDump(const ProcessDumpRequest& request)
{
    SetCoreDumpServiceData(request.pid, request.tid, DfxRegs::CreateFromUcontext(request.context));
    StartCoreDump();
    WriteSegmentHeader();
    WriteNoteSegment();
}

void CoreDumpService::StartSecondStageDump(int32_t vmPid, const ProcessDumpRequest& request)
{
    if (!IsDoCoredump()) {
        return;
    }
    coreDumpThread_.vmPid = vmPid;
    int pid = coreDumpThread_.targetPid;
    WriteLoadSegment();
    WriteSectionHeader();
    auto ret = FinishCoreDump();
    if (!ret) {
        UnlinkFile(GetCoredumpFilePath());
    }
    std::string bundleName = ret ? GetCoredumpFileName() : "";
    int32_t retCode = ret ? ResponseCode::REQUEST_SUCCESS : ResponseCode::CORE_DUMP_GENERATE_FAIL;
    FinishCoredumpCb(pid, bundleName, retCode);
    if (IsHwasanCoredumpEnabled()) {
        DumpUtils::InfoCrashUnwindResult(request, true);
    }
    exit(0);
}

std::string CoreDumpService::GetBundleNameItem()
{
    return bundleName_;
}

CoreDumpThread CoreDumpService::GetCoreDumpThread()
{
    return coreDumpThread_;
}

bool CoreDumpService::StartCoreDump()
{
    DFXLOGI("Begin to start coredump");
    if (status_ != WriteStatus::START_STAGE) {
        DFXLOGE("The status is not START_STAGE!");
        return false;
    }
    if (!CreateFile()) {
        return false;
    }
    if (!MmapForFd()) {
        return false;
    }
    status_ = WriteStatus::WRITE_SEGMENT_HEADER_STAGE;
    return true;
}

bool CoreDumpService::FinishCoreDump()
{
    DFXLOGI("Coredump end: pid = %{public}d, elapsed time = %{public}" PRId64 "ms",
        coreDumpThread_.targetPid, counter_.Elapsed<std::chrono::milliseconds>());
    if (status_ != WriteStatus::STOP_STAGE) {
        DFXLOGE("The status is not STOP_STAGE!");
        return false;
    }
    realCoreFileSize_ = static_cast<uint64_t>(currentPointer_ - mappedMemory_);
    if (!AdjustFileSize(fd_, realCoreFileSize_)) {
        return false;
    }
    status_ = WriteStatus::DONE_STAGE;
    return true;
}

void CoreDumpService::DeInit()
{
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
    if (mappedMemory_ != nullptr) {
        munmap(static_cast<void*>(mappedMemory_), coreFileSize_);
        mappedMemory_ = nullptr;
    }
}

bool CoreDumpService::CreateFile()
{
    if (coreDumpThread_.targetPid == 0) {
        DFXLOGE("The targetPid is 0!");
        return false;
    }
    coreFileSize_ = GetCoreFileSize(coreDumpThread_.targetPid);
    if (coreFileSize_ == 0) {
        DFXLOGE("corefile size is 0");
        return false;
    }
    fd_ = CreateFileForCoreDump();
    if (fd_ == INVALID_FD) {
        DFXLOGE("create file fail");
        return false;
    }
    return true;
}

bool CoreDumpService::MmapForFd()
{
    if (fd_ == -1) {
        DFXLOGE("The fd is invalid, not to mmap");
        return false;
    }
    if (coreFileSize_ == 0) {
        DFXLOGE("The coreFileSize is 0, not to mmap");
        return false;
    }
    if (!AdjustFileSize(fd_, coreFileSize_)) {
        return false;
    }
    mappedMemory_ = static_cast<char *>(mmap(nullptr, coreFileSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (mappedMemory_ == MAP_FAILED) {
        DFXLOGE("mmap fail");
        mappedMemory_ = nullptr;
        return false;
    }
    DFXLOGI("mmap success");
    return true;
}

bool CoreDumpService::WriteSegmentHeader()
{
    DFXLOGI("Begin to Write segment header");
    if (status_ != WriteStatus::WRITE_SEGMENT_HEADER_STAGE) {
        DFXLOGE("The status is not WRITE_SEGMENT_HEADER_STAGE!");
        return false;
    }
    currentPointer_ = mappedMemory_;
    currentPointer_ += sizeof(Elf64_Ehdr);
    ProgramSegmentHeaderWriter programSegmentHeader(mappedMemory_, currentPointer_, ePhnum_, maps_);
    currentPointer_ = programSegmentHeader.Write();
    Elf64_Ehdr eh;
    ElfHeaderFill(eh, ePhnum_);
    if (memcpy_s(mappedMemory_, sizeof(eh), reinterpret_cast<char*>(&eh), sizeof(eh)) != EOK) {
        DFXLOGE("memcpy fail");
        return false;
    }
    status_ = WriteStatus::WRITE_NOTE_SEGMENT_STAGE;
    return true;
}

bool CoreDumpService::WriteNoteSegment()
{
    DFXLOGI("Begin to Write note segment");
    if (status_ != WriteStatus::WRITE_NOTE_SEGMENT_STAGE) {
        DFXLOGE("The status is not WRITE_NOTE_STAGE!");
        return false;
    }
    if (coreDumpThread_.targetPid == 0 || coreDumpThread_.targetTid == 0) {
        DFXLOGE("targetPid or targetTid is 0!");
        return false;
    }

    NoteSegmentWriter note(mappedMemory_, currentPointer_, coreDumpThread_, maps_, keyRegs_);
    note.SetKeyThreadData(coreDumpKeyThreadData_);
    currentPointer_ = note.Write();
    status_ = WriteStatus::WRITE_LOAD_SEGMENT_STAGE;
    return true;
}

bool CoreDumpService::WriteLoadSegment()
{
    DFXLOGI("Begin to Write load segment");
    if (status_ != WriteStatus::WRITE_LOAD_SEGMENT_STAGE) {
        DFXLOGE("The status is not WRITE_SEGMENT_STAGE!");
        return false;
    }
    if (coreDumpThread_.vmPid == 0) {
        DFXLOGE("vmPid is 0!");
        return false;
    }
    LoadSegmentWriter segment(mappedMemory_, currentPointer_, coreDumpThread_.vmPid, ePhnum_);
    currentPointer_ = segment.Write();
    status_ = WriteStatus::WRITE_SECTION_HEADER_STAGE;
    return true;
}

bool CoreDumpService::WriteSectionHeader()
{
    DFXLOGI("Begin to Write section header");
    if (status_ != WriteStatus::WRITE_SECTION_HEADER_STAGE) {
        DFXLOGE("The status is not WRITE_SECTION_HEADER_STAGE!");
        return false;
    }
    SectionHeaderTableWriter sectionHeaderTable(mappedMemory_, currentPointer_);
    currentPointer_ = sectionHeaderTable.Write();
    status_ = WriteStatus::STOP_STAGE;
    return true;
}

void CoreDumpService::ElfHeaderFill(Elf64_Ehdr &eh, uint16_t ePhnum)
{
    eh.e_ident[EI_MAG0] = ELFMAG0;
    eh.e_ident[EI_MAG1] = ELFMAG1;
    eh.e_ident[EI_MAG2] = ELFMAG2;
    eh.e_ident[EI_MAG3] = ELFMAG3;
    eh.e_ident[EI_CLASS] = ELFCLASS64;
    eh.e_ident[EI_DATA] = ELFDATA2LSB;
    eh.e_ident[EI_VERSION] = EV_CURRENT;
    eh.e_ident[EI_OSABI] = ELFOSABI_NONE;
    eh.e_ident[EI_ABIVERSION] = 0x00;
    eh.e_ident[EI_PAD] = 0x00;
    eh.e_ident[10] = 0x00; // 10
    eh.e_ident[11] = 0x00; // 11
    eh.e_ident[12] = 0x00; // 12
    eh.e_ident[13] = 0x00; // 13
    eh.e_ident[14] = 0x00; // 14
    eh.e_ident[15] = 0x00; // 15
    eh.e_type = ET_CORE;
    eh.e_machine = EM_AARCH64;
    eh.e_version = EV_CURRENT;
    eh.e_entry = 0x00;
    eh.e_phoff = sizeof(Elf64_Ehdr);
    eh.e_shoff = sizeof(Elf64_Shdr);
    eh.e_flags = 0x00;
    eh.e_ehsize = sizeof(Elf64_Ehdr);
    eh.e_phentsize = sizeof(Elf64_Phdr);
    eh.e_phnum = ePhnum;
    eh.e_shentsize = sizeof(Elf64_Shdr);
    eh.e_shnum = ePhnum + ARG2;
    eh.e_shstrndx = ePhnum + 1;
}

bool CoreDumpService::IsDoCoredump()
{
    if (VerifyTrustlist() || (IsHwasanCoredumpEnabled() && isHwasanHap_)) {
        return true;
    }
    DFXLOGE("The bundleName %{public}s is not in whitelist or hwasan coredump disable", bundleName_.c_str());
    return false;
}

bool CoreDumpService::IsCoredumpAllowed(const ProcessDumpRequest& request)
{
    if (IsCoredumpSignal(request) || (request.siginfo.si_signo == SIGABRT && IsHwasanCoredumpEnabled())) {
        return true;
    }
    return false;
}

bool CoreDumpService::GetKeyThreadData(const ProcessDumpRequest& request)
{
    pid_t tid = request.tid;
    if (tid == 0) {
        DFXLOGE("The keythread tid is 0, not to get keythread data");
        return false;
    }

    UserPacMask ntUserPacMask;
    NoteSegmentWriter::GetRegset(tid, NT_ARM_PAC_MASK, ntUserPacMask);
    coreDumpKeyThreadData_.ntUserPacMask = ntUserPacMask;

    struct user_fpsimd_struct ntFpregset;
    if (NoteSegmentWriter::GetRegset(tid, NT_FPREGSET, ntFpregset)) {
        coreDumpKeyThreadData_.fpRegValid = 1;
    } else {
        coreDumpKeyThreadData_.fpRegValid = 0;
    }
    coreDumpKeyThreadData_.ntFpregset = ntFpregset;

    siginfo_t ntSiginfo;
    NoteSegmentWriter::GetSiginfoCommon(ntSiginfo, tid);
    coreDumpKeyThreadData_.ntSiginfo = ntSiginfo;

    prstatus_t ntPrstatus;
    if (NoteSegmentWriter::GetSiginfoCommon(ntPrstatus.pr_info, tid)) {
        coreDumpKeyThreadData_.prStatusValid = 1;
    } else {
        coreDumpKeyThreadData_.prStatusValid = 0;
    }
    coreDumpKeyThreadData_.ntPrstatus = ntPrstatus;
    return true;
}

int CoreDumpService::CreateFileForCoreDump()
{
    bundleName_ = DumpUtils::GetSelfBundleName();
    if (bundleName_.empty()) {
        DFXLOGE("query bundleName fail");
        return INVALID_FD;
    }
    if (!IsDoCoredump()) {
        return INVALID_FD;
    }
    std::string logPath = GetCoredumpFilePath();
    RegisterCancelCoredump(logPath);
    StartCoredumpCb(coreDumpThread_.targetPid, getpid());
    if (access(COREDUMP_DIR_PATH, F_OK) != 0) {
        DFXLOGE("%{public}s is not exist, errno = %{public}d", COREDUMP_DIR_PATH, errno);
        return INVALID_FD;
    }

    int fd = OHOS_TEMP_FAILURE_RETRY(open(logPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR));
    if (fd == INVALID_FD) {
        DFXLOGE("create %{public}s fail, errno = %{public}d", logPath.c_str(), errno);
    } else {
        DFXLOGI("create coredump path %{public}s succ", logPath.c_str());
    }
    return fd;
}

bool CoreDumpService::VerifyTrustlist()
{
    if (bundleName_.empty()) {
        return false;
    }
    std::string whiteList = GetCoredumpWhiteList();
    if (whiteList.find(bundleName_) != std::string::npos) {
        return true;
    }
    return false;
}

bool CoreDumpService::AdjustFileSize(int fd, uint64_t fileSize)
{
    if (fd == -1) {
        DFXLOGE("fd is invalid, not to adjust file size");
        return false;
    }
    if (fileSize == 0) {
        DFXLOGE("filesize is 0, not to adjust file size");
        return false;
    }
    if (ftruncate(fd, fileSize) == -1) {
        DFXLOGE("ftruncate fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

uint64_t CoreDumpService::GetCoreFileSize(pid_t pid)
{
    uint64_t coreFileSize = 0;
    if (pid == 0) {
        return coreFileSize;
    }
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream file(path);
    if (!file.is_open()) {
        DFXLOGE("open %{public}s fail", path.c_str());
        return coreFileSize;
    }

    std::string line;
    uint16_t lineNumber = 0;
    DumpMemoryRegions region;
    while (getline(file, line)) {
        if (!isHwasanHap_ && line.find("libclang_rt.hwasan.so") != std::string::npos) {
            isHwasanHap_ = true;
        }
        lineNumber += 1;
        ObtainDumpRegion(line, region);
        maps_.push_back(region);
        std::string pri(region.priority);
        if (pri.find('r') == std::string::npos || pri.find('p') == std::string::npos) {
            continue;
        }
        coreFileSize += region.memorySizeHex;
    }

    file.close();
    coreFileSize = coreFileSize + sizeof(Elf64_Ehdr) + (lineNumber + 1) * sizeof(Elf64_Phdr) +
        (lineNumber + 2) * sizeof(Elf64_Shdr) + lineNumber * (sizeof(Elf64_Nhdr) + ARG100) + ARG1000; // 2
    DFXLOGI("The estimated corefile size is: %{public}ld, is hwasan hap %{public}d", coreFileSize, isHwasanHap_);
    return coreFileSize;
}

bool CoreDumpService::UnlinkFile(const std::string &logPath)
{
    if (logPath.empty()) {
        return false;
    }
    if (unlink(logPath.c_str()) != 0) {
        DFXLOGI("unlink file(%{public}s) fail, errno:%{public}d", logPath.c_str(), errno);
        return false;
    }
    DFXLOGI("unlink file(%{public}s) success", logPath.c_str());
    return true;
}

std::string CoreDumpService::GetCoredumpFileName()
{
    if (bundleName_.empty()) {
        return "";
    }
    return bundleName_ + ".dmp";
}

std::string CoreDumpService::GetCoredumpFilePath()
{
    if (bundleName_.empty()) {
        return "";
    }
    return std::string(COREDUMP_DIR_PATH) + "/" + GetCoredumpFileName();
}

void CoreDumpService::ObtainDumpRegion(std::string &line, DumpMemoryRegions &region)
{
    auto ret = sscanf_s(line.c_str(), "%[^-\n]-%[^ ] %s %s %s %u %[^\n]",
        region.start, sizeof(region.start),
        region.end, sizeof(region.end),
        region.priority, sizeof(region.priority),
        region.offset, sizeof(region.offset),
        region.dev, sizeof(region.dev),
        &(region.inode),
        region.pathName, sizeof(region.pathName));
    if (ret != 7) { // 7
        region.pathName[0] = '\0';
    }

    region.startHex = strtoul(region.start, nullptr, ARG16);
    region.endHex = strtoul(region.end, nullptr, ARG16);
    region.offsetHex = strtoul(region.offset, nullptr, ARG16);
    region.memorySizeHex = region.endHex - region.startHex;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif