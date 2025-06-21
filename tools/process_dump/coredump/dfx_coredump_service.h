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

#include "dfx_coredump_common.h"
#include "dfx_coredump_writer.h"
#include "dfx_dump_request.h"
#include "dfx_regs.h"
#include "elapsed_time.h"

namespace OHOS {
namespace HiviewDFX {
class CoreDumpService {
public:
    CoreDumpService() = default;
    CoreDumpService(int32_t targetPid, int32_t targetTid);
    CoreDumpService(int32_t targetPid, int32_t targetTid, std::shared_ptr<DfxRegs> keyRegs);
    CoreDumpService(const CoreDumpService&) = delete;
    CoreDumpService &operator=(const CoreDumpService&) = delete;
    ~CoreDumpService();
    bool StartCoreDump();
    bool FinishCoreDump();
    bool WriteSegmentHeader();
    bool WriteNoteSegment();
    bool WriteLoadSegment();
    bool WriteSectionHeader();
    void SetVmPid(int32_t vmPid);
    void StartFirstStageDump();
    void StartSecondStageDump(int32_t vmPid, const ProcessDumpRequest& request);
    CoreDumpThread GetCoreDumpThread();
    std::string GetBundleNameItem();
    static bool IsHwasanCoredumpEnabled();
    static bool IsCoredumpSignal(const ProcessDumpRequest& request);
private:
    void DeInit();
    void ObtainDumpRegion(std::string &line, DumpMemoryRegions &region);
    void ElfHeaderFill(Elf64_Ehdr &eh, uint16_t ePhnum);
    bool CreateFile();
    bool MmapForFd();
    bool VerifyTrustlist();
    int CreateFileForCoreDump();
    uint64_t GetCoreFileSize(pid_t pid);
private:
    enum class WriteStatus {
        START_STAGE,
        WRITE_SEGMENT_HEADER_STAGE,
        WRITE_NOTE_SEGMENT_STAGE,
        WRITE_LOAD_SEGMENT_STAGE,
        WRITE_SECTION_HEADER_STAGE,
        STOP_STAGE,
        DONE_STAGE,
    };
    int fd_ {-1};
    uint64_t coreFileSize_ {0};
    Elf64_Half ePhnum_ {0};
    std::vector<DumpMemoryRegions> maps_;
    std::string bundleName_;
    char *mappedMemory_ {nullptr};
    char *currentPointer_ {nullptr};
    WriteStatus status_ = WriteStatus::START_STAGE;
    CoreDumpThread coreDumpThread_;
    ElapsedTime counter_;
    std::shared_ptr<DfxRegs> keyRegs_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
#endif