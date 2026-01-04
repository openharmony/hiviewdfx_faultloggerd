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
#include "coredump_load_segment_writer.h"

#include <thread>

#include <sys/uio.h>

#include "coredump_config_manager.h"
#include "dfx_log.h"
#include "dfx_trace.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {

auto LoadSegmentWriter::MergeLoadSeg(size_t idxBegin, size_t idxEnd)
{
    std::vector<MergedSegment> mergedSegments;
    for (size_t i = idxBegin; i < idxEnd;) {
        Elf64_Phdr merged = segs_[i];
        size_t j = i + 1;
        while (j < segs_.size() &&
               (merged.p_vaddr + merged.p_memsz == segs_[j].p_vaddr) &&
               (merged.p_flags == segs_[j].p_flags)) {
            merged.p_memsz += segs_[j].p_memsz;
            merged.p_filesz += segs_[j].p_filesz;
            ++j;
        }
        mergedSegments.push_back({ merged, i, j });
        i = j;
    }
    return mergedSegments;
}

uint64_t LoadSegmentWriter::WriteMergedSeg(size_t idxBegin, size_t idxEnd, char *start)
{
    DFX_TRACE_SCOPED("WriteLoadSeg");
    char* base = bw_.GetBase();
    std::vector<MergedSegment> mergedSegs = MergeLoadSeg(idxBegin, idxEnd);
    char* ptLoadAddr = bw_.GetBase() + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    Elf64_Phdr *startLoadAddr = reinterpret_cast<Elf64_Phdr *>(ptLoadAddr);

    char *oldStart = start;
    for (; idxBegin < idxEnd; idxBegin++) {
        auto seg = segs_[idxBegin];
        (startLoadAddr + idxBegin)->p_offset =  reinterpret_cast<uintptr_t>(start) - reinterpret_cast<uintptr_t>(base);
        start += seg.p_memsz;
    }
    uint64_t totalSize = 0;
    int count = 0;
    ElapsedTime counter;
    for (const auto& seg: mergedSegs) {
        const auto& phdr = seg.phdr;
        if (oldStart + phdr.p_memsz > bw_.GetBase() + bw_.GetCapacity()) {
            DFXLOGE("buffer no space to write load seg");
            break;
        }

        struct iovec local = { oldStart, phdr.p_memsz };
        struct iovec remote =  { reinterpret_cast<void*>(phdr.p_vaddr), phdr.p_memsz };
        DFX_TRACE_SCOPED("WriteLoadSeg %d", ++count);
        process_vm_readv(pid_, &local, 1, &remote, 1, 0);
        oldStart += phdr.p_memsz;
        totalSize += phdr.p_memsz;
    }
    DFXLOGI("write load elapsed time = %{public}" PRId64 "ms count %{public}d total size%{public}lu",
        counter.Elapsed<std::chrono::milliseconds>(), count, totalSize);
    return totalSize;
}

size_t LoadSegmentWriter::GetTotalSize()
{
    size_t total = 0;
    for (const auto &seg: segs_) {
        total += seg.p_memsz;
    }
    return total;
}

int LoadSegmentWriter::GetDumpThreadCount()
{
    int threadCount = CoredumpConfigManager::GetInstance().GetConfig().dumperThreadCount;
    if (threadCount <= 0) {
        threadCount = 1;
    }
    constexpr int maxThreadCount = 8;
    if (threadCount > maxThreadCount) {
        threadCount = maxThreadCount;
    }
    return threadCount;
}

auto LoadSegmentWriter::GenerateWriteTasks()
{
    char* base = bw_.GetCurrent();

    std::vector<WriteTask> tasks;
    auto threadCount = GetDumpThreadCount();
    size_t totalSize = GetTotalSize();
    size_t targetSize = totalSize / static_cast<size_t>(threadCount);

    size_t offset = 0;
    size_t begin = 0;

    for (int i = 0; i < threadCount; ++i) {
        size_t accSize = 0;
        size_t end = begin;

        while (end < segs_.size() && (i == threadCount - 1 || accSize + segs_[end].p_memsz <= targetSize)) {
            accSize += segs_[end].p_memsz;
            ++end;
        }

        tasks.push_back({ begin, end, base + offset });
        offset += accSize;
        begin = end;
    }

    return tasks;
}

void LoadSegmentWriter::RunWriteTasks(std::vector<WriteTask>& tasks)
{
    std::vector<std::thread> threads;

    for (auto& task : tasks) {
        threads.emplace_back([&task, this]() {
            task.totalWritten = WriteMergedSeg(task.from, task.to,  task.dst);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    for (const auto& task : tasks) {
        bw_.Advance(task.totalWritten);
    }
}

bool LoadSegmentWriter::Write()
{
    char *ptLoadAddr = bw_.GetBase() + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    Elf64_Phdr *ptLoad = reinterpret_cast<Elf64_Phdr *>(ptLoadAddr);
    if (ptLoad == nullptr) {
        return false;
    }
    for (Elf64_Half i = 0; i < (ePhnum_ - 1); i++) {
        segs_.push_back(*ptLoad);
        ptLoad += 1;
    }

    auto tasks = GenerateWriteTasks();
    RunWriteTasks(tasks);

    DFXLOGI("write load segment elapsed time = %{public}" PRId64 "ms", counter_.Elapsed<std::chrono::milliseconds>());
    return true;
}
}
}
