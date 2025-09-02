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
#ifndef COREDUMP_DUMP_GENERATOR_H
#define COREDUMP_DUMP_GENERATOR_H

#include "coredump_buffer_writer.h"
#include "coredump_file_manager.h"
#include "coredump_note_segment_writer.h"
#include "dfx_dump_request.h"
#include "dfx_regs.h"
#include "elapsed_time.h"

namespace OHOS {
namespace HiviewDFX {
class ICoredumpGenerator {
public:
    virtual ~ICoredumpGenerator() = default;
    virtual bool TriggerCoredump() = 0;
    virtual void DumpMemoryForPid(pid_t pid) = 0;
};

/*
 * ELF core dump layout (ELF64, ET_CORE)
 * --------------------------------------
 * |            ELF Header              | : e_type=ET_CORE, e_shoff=0 (usually)
 * --------------------------------------
 * |       Program Header Table         | : array of Elf64_Phdr
 * --------------------------------------
 * |            PT_NOTE #0              | : NT_PRSTATUS, NT_PRPSINFO, NT_AUXV, NT_FILE, ...
 * --------------------------------------
 * |         PT_NOTE #N (optional)      | : Optional: thread info, arch-specific notes (e.g. PAC)
 * --------------------------------------
 * |            PT_LOAD #0              | : Memory snapshot of VMA #0
 * --------------------------------------
 * |            PT_LOAD #1              | :  Memory snapshot of VMA #1
 * --------------------------------------
 * |                 ...                |
 * --------------------------------------
 * |           Section Table            | :  Option
 * --------------------------------------
 */
class CoredumpGenerator : public ICoredumpGenerator {
public:
    explicit CoredumpGenerator(const ProcessDumpRequest& request);
    CoredumpGenerator(const CoredumpGenerator&) = delete;
    CoredumpGenerator &operator=(const CoredumpGenerator&) = delete;

    bool TriggerCoredump() override;
    void DumpMemoryForPid(pid_t vmPid) override;
private:
    bool MmapCoredumpFile();
    bool AdjustCoredumpFile();

    bool WriteElfHeader();
    bool WriteSegmentHeader();
    bool WriteNoteSegment();
    bool WriteLoadSegment(pid_t vmPid);
    bool WriteSectionHeader();
    virtual bool LoadConfig() = 0;
    void FinishCoredump(bool ret);

    Elf64_Half ePhnum_ {0};
    CoredumpProc coredumpProc_;
    ElapsedTime counter_;
    CoredumpThread coredumpKeyThread_;
    CoredumpFileManager fileMgr_;
    std::unique_ptr<CoredumpBufferWriter> bw_;
    ProcessDumpRequest request_;
};

class FullCoredumpGenerator : public CoredumpGenerator {
public:
    explicit FullCoredumpGenerator(const ProcessDumpRequest& request) :CoredumpGenerator(request) {}
    bool LoadConfig() override;
};

class HwAsanDumpGenerator : public CoredumpGenerator {
public:
    explicit HwAsanDumpGenerator(const ProcessDumpRequest& request) : CoredumpGenerator(request) {}
    bool LoadConfig() override;
};
}
}
#endif
