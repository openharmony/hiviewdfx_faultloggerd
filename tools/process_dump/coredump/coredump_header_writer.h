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
#ifndef COREDUMP_HEADER_WRITER_H
#define COREDUMP_HEADER_WRITER_H

#include "coredump_buffer_writer.h"
#include "coredump_common.h"
#include "coredump_load_segment_writer.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpElfHeaderWriter : public Writer {
public:
    explicit CoredumpElfHeaderWriter(CoredumpBufferWriter& bufferWriter) : bufferWriter_(bufferWriter) {}
    void InitElfHeader(uint16_t ePhnum);
    bool UpdatePhnum(uint16_t ePhnum);
    bool Write() override;
private:
    CoredumpBufferWriter& bufferWriter_;
    Elf64_Ehdr ehdr_ {};
};

class ProgramSegmentHeaderWriter : public Writer {
public:
    ProgramSegmentHeaderWriter(const std::vector<DumpMemoryRegions>& maps, CoredumpBufferWriter& bw)
        : maps_(maps), bw_(bw) {}
    bool Write() override;
    Elf64_Half GetPhnum() const { return ePhnum_; }
private:
    static void ProgramSegmentHeaderFill(Elf64_Phdr &ph, Elf64_Word pType, Elf64_Word pFlags,
        const DumpMemoryRegions &region);
    static void PtNoteHeaderFill(Elf64_Phdr &ph);
    static void PtLoadHeaderFill(Elf64_Phdr &ph, const DumpMemoryRegions &region);
    Elf64_Half ePhnum_ {0};
    std::vector<DumpMemoryRegions> maps_;
    CoredumpBufferWriter& bw_;
};

class SectionHeaderTableWriter : public Writer {
public:
    explicit SectionHeaderTableWriter(CoredumpBufferWriter& bw) : bw_(bw) {}
    bool Write() override;
private:
    void SectionHeaderFill(Elf64_Shdr *sectionHeader, Elf64_Word shType, Elf64_Xword shFlag, Elf64_Phdr *programHeader);
    void AdjustOffset(uint8_t remain);
    static void SetShFlag(const Elf64_Phdr* programHeader, Elf64_Xword &shFlag);
    bool WriteSystemVersion(Elf64_Shdr *sectionHeader, char *versionStartAddr, size_t verLen);
    CoredumpBufferWriter& bw_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
