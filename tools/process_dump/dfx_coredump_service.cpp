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
const char *const DEFAULT_BUNDLE_NAME = "";
const char *const HWASAN_COREDUMP_ENABLE = "faultloggerd.priv.hwasan_coredump.enabled";
char g_coredumpFilePath[256] = {0};
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

CoreDumpService::CoreDumpService(int32_t targetPid, int32_t targetTid)
{
    coreDumpThread_.targetPid = targetPid;
    coreDumpThread_.targetTid = targetTid;
}

void CoreDumpService::SetVmPid(int32_t vmPid)
{
    coreDumpThread_.vmPid = vmPid;
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
    Init();
    if (!CreateFile()) {
        return false;
    }
    if (!MmapForFd()) {
        return false;
    }
    return true;
}

void CoreDumpService::StopCoreDump()
{
    DeInit();
}

void CoreDumpService::Init()
{
    fd_ = -1;
    mappedMemory_ = nullptr;
    currentPointer_ = nullptr;
    ePhnum_ = 0;
    coreFileSize_ = 0;
    status_ = WriteStatus::INIT_STAGE;
}

void CoreDumpService::DeInit()
{
    if (status_ != WriteStatus::DEINIT_STAGE) {
        DFXLOGE("The status is not DEINIT_STAGE!");
        return;
    }
    if (fd_ != 0) {
        close(fd_);
        fd_ = 0;
    }
    if (mappedMemory_ != nullptr) {
        munmap(static_cast<void*>(mappedMemory_), coreFileSize_);
    }
    status_ = WriteStatus::DONE_STAGE;
}

bool CoreDumpService::CreateFile()
{
    if (status_ != WriteStatus::INIT_STAGE || coreDumpThread_.targetPid == 0) {
        DFXLOGE("The status is not INIT_STAGE!");
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
    DFXLOGI("create coredump file success");
    status_ = WriteStatus::MMAP_STAGE;
    return true;
}

bool CoreDumpService::MmapForFd()
{
    if (status_ != WriteStatus::MMAP_STAGE) {
        DFXLOGE("The status is not MMAP_STAGE!");
        return false;
    }
    if (ftruncate(fd_, coreFileSize_) == -1) {
        DFXLOGE("ftruncate fail");
        close(fd_);
        fd_ = 0;
        return false;
    }
    mappedMemory_ = static_cast<char *>(mmap(nullptr, coreFileSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (mappedMemory_ == MAP_FAILED) {
        DFXLOGE("mmap fail");
        close(fd_);
        fd_ = 0;
        mappedMemory_ = nullptr;
        return false;
    }
    DFXLOGI("mmap success");
    status_ = WriteStatus::WRITE_SEGMENT_HEADER_STAGE;
    return true;
}

bool CoreDumpService::WriteSegmentHeader()
{
    if (status_ != WriteStatus::WRITE_SEGMENT_HEADER_STAGE) {
        DFXLOGE("The status is not WRITE_SEGMENT_HEADER_STAGE!");
        return false;
    }
    currentPointer_ = mappedMemory_;
    currentPointer_ += sizeof(Elf64_Ehdr);
    ProgramSegmentHeaderWriter programSegmentHeader(mappedMemory_, currentPointer_, &ePhnum_, maps_);
    currentPointer_ = programSegmentHeader.Write();
    Elf64_Ehdr eh;
    ElfHeaderFill(&eh, ePhnum_, ePhnum_ + 2, ePhnum_ + 1); // 2
    if (memcpy_s(mappedMemory_, sizeof(eh), reinterpret_cast<char*>(&eh), sizeof(eh)) != EOK) {
        DFXLOGE("memcpy fail");
        return false;
    }
    status_ = WriteStatus::WRITE_NOTE_STAGE;
    return true;
}

bool CoreDumpService::WriteNote()
{
    if (status_ != WriteStatus::WRITE_NOTE_STAGE) {
        DFXLOGE("The status is not WRITE_NOTE_STAGE!");
        return false;
    }
    if (coreDumpThread_.targetPid == 0 || coreDumpThread_.targetTid == 0) {
        DFXLOGE("targetPid or targetTid is 0!");
        return false;
    }

    NoteWriter note(mappedMemory_, currentPointer_, coreDumpThread_, maps_, keyRegs_);
    currentPointer_ = note.Write();
    status_ = WriteStatus::WRITE_SEGMENT_STAGE;
    return true;
}

bool CoreDumpService::WriteSegment()
{
    if (status_ != WriteStatus::WRITE_SEGMENT_STAGE) {
        DFXLOGE("The status is not WRITE_SEGMENT_STAGE!");
        return false;
    }
    if (coreDumpThread_.vmPid == 0) {
        DFXLOGE("vmPid is 0!");
        return false;
    }
    SegmentWriter segment(mappedMemory_, currentPointer_, coreDumpThread_.vmPid, ePhnum_);
    currentPointer_ = segment.Write();
    status_ = WriteStatus::WRITE_SECTION_HEADER_STAGE;
    return true;
}

bool CoreDumpService::WriteSectionHeader()
{
    if (status_ != WriteStatus::WRITE_SECTION_HEADER_STAGE) {
        DFXLOGE("The status is not WRITE_SECTION_HEADER_STAGE!");
        return false;
    }
    SectionHeaderTableWriter sectionHeaderTable(mappedMemory_, currentPointer_);
    currentPointer_ = sectionHeaderTable.Write();
    status_ = WriteStatus::DEINIT_STAGE;
    return true;
}

void CoreDumpService::ElfHeaderFill(Elf64_Ehdr* eh, uint16_t ePhnum, uint16_t eShnum, uint16_t eShstrndx)
{
    eh->e_ident[EI_MAG0] = ELFMAG0;
    eh->e_ident[EI_MAG1] = ELFMAG1;
    eh->e_ident[EI_MAG2] = ELFMAG2;
    eh->e_ident[EI_MAG3] = ELFMAG3;
    eh->e_ident[EI_CLASS] = ELFCLASS64;
    eh->e_ident[EI_DATA] = ELFDATA2LSB;
    eh->e_ident[EI_VERSION] = EV_CURRENT;
    eh->e_ident[EI_OSABI] = ELFOSABI_NONE;
    eh->e_ident[EI_ABIVERSION] = 0x00;
    eh->e_ident[EI_PAD] = 0x00;
    eh->e_ident[10] = 0x00; // 10
    eh->e_ident[11] = 0x00; // 11
    eh->e_ident[12] = 0x00; // 12
    eh->e_ident[13] = 0x00; // 13
    eh->e_ident[14] = 0x00; // 14
    eh->e_ident[15] = 0x00; // 15
    eh->e_type = ET_CORE;
    eh->e_machine = EM_AARCH64;
    eh->e_version = EV_CURRENT;
    eh->e_entry = 0x00;
    eh->e_phoff = sizeof(Elf64_Ehdr);
    eh->e_shoff = sizeof(Elf64_Shdr);
    eh->e_flags = 0x00;
    eh->e_ehsize = sizeof(Elf64_Ehdr);
    eh->e_phentsize = sizeof(Elf64_Phdr);
    eh->e_phnum = ePhnum;
    eh->e_shentsize = sizeof(Elf64_Shdr);
    eh->e_shnum = eShnum;
    eh->e_shstrndx = eShstrndx;
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

int CoreDumpService::CreateFileForCoreDump()
{
    bundleName_ = GetBundleName();
    if (bundleName_.empty()) {
        DFXLOGE("query bundleName fail");
        return INVALID_FD;
    }
    if (!VerifyTrustlist() && !IsHwasanCoredumpEnabled()) {
        DFXLOGE("The bundleName %{public}s is not in whitelist or hwasan coredump disable", bundleName_.c_str());
        return INVALID_FD;
    }
    std::string logPath = std::string(COREDUMP_DIR_PATH) + "/" + bundleName_ + ".dmp";
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
    struct DumpMemoryRegions region;
    while (getline(file, line)) {
        lineNumber += 1;
        ObtainDumpRegion(line.c_str(), &region);
        maps_.push_back(region);
        if (!strcmp(region.pathName, "[vvar]") || region.priority[0] != 'r') {
            continue;
        }
        coreFileSize += region.memorySizeHex;
    }

    file.close();
    coreFileSize = coreFileSize + sizeof(Elf64_Ehdr) + (lineNumber + 1) * sizeof(Elf64_Phdr) +
        (lineNumber + 2) * sizeof(Elf64_Shdr) + lineNumber * (sizeof(Elf64_Nhdr) + ARG100) + ARG1000; // 2
    DFXLOGI("The estimated corefile size is: %{public}ld", coreFileSize);
    return coreFileSize;
}

void CoreDumpService::ObtainDumpRegion(const char *line, DumpMemoryRegions *region)
{
    auto ret = sscanf_s(line, "%[^-\n]-%[^ ] %s %s %s %u %[^\n]",
        region->start, sizeof(region->start),
        region->end, sizeof(region->end),
        region->priority, sizeof(region->priority),
        region->offset, sizeof(region->offset),
        region->dev, sizeof(region->dev),
        &(region->inode),
        region->pathName, sizeof(region->pathName));
    if (ret != 7) { // 7
        region->pathName[0] = '\0';
    }

    region->startHex = strtoul(region->start, nullptr, ARG16);
    region->endHex = strtoul(region->end, nullptr, ARG16);
    region->offsetHex = strtoul(region->offset, nullptr, ARG16);
    region->memorySizeHex = region->endHex - region->startHex;
}

std::string CoreDumpService::GetBundleName()
{
#ifndef is_ohos_lite
    auto bundleInstance = GetBundleManager();
    if (bundleInstance == nullptr) {
        DFXLOGE("bundleInstance is nullptr");
        return DEFAULT_BUNDLE_NAME;
    }
    AppExecFwk::BundleInfo bundleInfo;
    auto ret = bundleInstance->GetBundleInfoForSelf(0, bundleInfo);
    if (ret != ERR_OK) {
        DFXLOGE("GetBundleInfoForSelf failed! ret = %{public}d", ret);
        return DEFAULT_BUNDLE_NAME;
    }
    return bundleInfo.name;
#endif
    return DEFAULT_BUNDLE_NAME;
}

#ifndef is_ohos_lite
sptr<AppExecFwk::IBundleMgr> CoreDumpService::GetBundleManager()
{
    auto systemManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemManager) {
        DFXLOGE("Get system ability manager failed");
        return nullptr;
    }
    auto remoteObject = systemManager->GetSystemAbility(BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    if (!remoteObject) {
        DFXLOGE("Get system ability failed");
        return nullptr;
    }
    sptr<AppExecFwk::IBundleMgr> bundleMgrProxy = iface_cast<AppExecFwk::IBundleMgr>(remoteObject);
    return bundleMgrProxy;
}
#endif

} // namespace HiviewDFX
} // namespace OHOS
#endif