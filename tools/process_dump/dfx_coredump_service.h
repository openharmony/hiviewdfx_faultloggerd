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
#ifndef DFX_COREDUMP_SERVICE_H
#define DFX_COREDUMP_SERVICE_H
#if defined(__aarch64__)

#ifndef is_ohos_lite
#include "bundle_mgr_interface.h"
#include "bundle_mgr_proxy.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#endif
#include "dfx_coredump_common.h"
#include "dfx_coredump_writer.h"
#include "dfx_regs.h"

namespace OHOS {
namespace HiviewDFX {
std::string GetBundleName();

class CoreDumpService {
public:
    CoreDumpService() = default;
    CoreDumpService(int32_t targetPid, int32_t targetTid);
    CoreDumpService(const CoreDumpService&) = delete;
    bool StartCoreDump();
    void StopCoreDump();
    bool WriteSegmentHeader();
    bool WriteNote();
    bool WriteSegment();
    bool WriteSectionHeader();
    void SetVmPid(int32_t vmPid);
    CoreDumpThread GetCoreDumpThread();
    std::string GetBundleNameItem();
    std::string GetBundleName();
    std::shared_ptr<DfxRegs> keyRegs_;
private:
    enum class WriteStatus {
        INIT_STAGE,
        MMAP_STAGE,
        WRITE_SEGMENT_HEADER_STAGE,
        WRITE_NOTE_STAGE,
        WRITE_SEGMENT_STAGE,
        WRITE_SECTION_HEADER_STAGE,
        DEINIT_STAGE,
        DONE_STAGE,
    };
    void Init();
    void DeInit();
    bool CreateFile();
    bool MmapForFd();
    bool VerifyTrustlist();
    int CreateFileForCoreDump();
    uint64_t GetCoreFileSize(pid_t pid);
    void ObtainDumpRegion(const char *line, DumpMemoryRegions *region);
    void ElfHeaderFill(Elf64_Ehdr* eh, uint16_t ePhnum, uint16_t eShnum, uint16_t eShstrndx);
#ifndef is_ohos_lite
    sptr<AppExecFwk::IBundleMgr> GetBundleManager();
#endif
    int fd_ {-1};
    uint64_t coreFileSize_ {0};
    std::vector<DumpMemoryRegions> maps_;
    char *mappedMemory_ {nullptr};
    std::string bundleName_;
    char *currentPointer_ {nullptr};
    Elf64_Half ePhnum_ {0};
    WriteStatus status_ = WriteStatus::INIT_STAGE;
    CoreDumpThread coreDumpThread_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
#endif