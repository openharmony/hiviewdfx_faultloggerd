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
#ifndef COREDUMP_LOAD_SEGMENT_WRITER_H
#define COREDUMP_LOAD_SEGMENT_WRITER_H

#include "coredump_common.h"

#include <elf.h>

#include "coredump_buffer_writer.h"
#include "coredump_writer.h"
#include "elapsed_time.h"

namespace OHOS {
namespace HiviewDFX {
class LoadSegmentWriter : public Writer {
public:
    LoadSegmentWriter(pid_t pid, Elf64_Half ePhnum, CoredumpBufferWriter& bw)
        : pid_(pid), ePhnum_(ePhnum), bw_(bw) {}
    bool Write() override;
private:
    struct WriteTask {
        size_t from {0};
        size_t to {0};
        char* dst {nullptr};
        uint64_t totalWritten {0};
    };
    uint64_t WriteMergedSeg(size_t idxBegin, size_t idxEnd, char *start);
    size_t GetTotalSize() const;
    auto GenerateWriteTasks();
    void RunWriteTasks(std::vector<WriteTask>& tasks);
    static int GetDumpThreadCount();

    struct MergedSegment {
        Elf64_Phdr phdr {};
        size_t originalIdxStart {0};
        size_t originalIdxEnd {0};
    };
    auto MergeLoadSeg(size_t idxBegin, size_t idxEnd);
    pid_t pid_ {0};
    Elf64_Half ePhnum_ {};
    CoredumpBufferWriter& bw_;
    ElapsedTime counter_;
    std::vector<Elf64_Phdr> segs_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
