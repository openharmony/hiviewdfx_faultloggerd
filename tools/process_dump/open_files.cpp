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
#include "decorative_dump_info.h"
#include <dirent.h>
#include "dfx_util.h"
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "unwinder.h"
#include "dfx_trace.h"
#include "dfx_buffer_writer.h"
#include "dfx_log.h"
namespace OHOS {
namespace HiviewDFX {
namespace {
#define FD_TABLE_SIZE 128
const int ARG_MAX_NUM = 131072;

struct FdEntry {
    _Atomic(uint64_t) close_tag;
    _Atomic(char) signal_flag;
};

struct FdTableOverflow {
    size_t len;
    struct FdEntry entries[];
};

struct FdTable {
    _Atomic(enum fdsan_error_level) error_level;
    struct FdEntry entries[FD_TABLE_SIZE];
    _Atomic(struct FdTableOverflow*) overflow;
};
}
REGISTER_DUMP_INFO_CLASS(OpenFiles);

void OpenFiles::Collect(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DFX_TRACE_SCOPED("GetOpenFiles");
    OpenFilesList openFies;
    CollectOpenFiles(openFies, process.GetProcessInfo().pid);
    FillFdsaninfo(openFies, process.GetProcessInfo().nsPid, request.fdTableAddr);
    openFilesStr_ = DumpOpenFiles(openFies);
    DFXLOGI("get open files info finish");
}

void OpenFiles::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    DfxBufferWriter::GetInstance().WriteMsg(openFilesStr_);
}

bool OpenFiles::ReadLink(std::string &src, std::string &dst)
{
    char buf[PATH_MAX];
    ssize_t count = readlink(src.c_str(), buf, sizeof(buf) - 1);
    if (count < 0) {
        return false;
    }
    buf[count] = '\0';
    dst = buf;
    return true;
}

void OpenFiles::CollectOpenFiles(OpenFilesList &list, pid_t pid)
{
    std::string fdDirName = "/proc/" + std::to_string(pid) + "/fd";
    std::unique_ptr<DIR, int (*)(DIR *)> dir(opendir(fdDirName.c_str()), closedir);
    if (dir == nullptr) {
        DFXLOGE("failed to open directory %{public}s: %{public}s", fdDirName.c_str(), strerror(errno));
        return;
    }

    struct dirent *de;
    while ((de = readdir(dir.get())) != nullptr) {
        if (*de->d_name == '.') {
            continue;
        }

        int fd = atoi(de->d_name);
        std::string path = fdDirName + "/" + std::string(de->d_name);
        std::string target;
        if (ReadLink(path, target)) {
            list[fd].path = target;
        } else {
            list[fd].path = "???";
            DFXLOGE("failed to readlink %{public}s: %{public}s", path.c_str(), strerror(errno));
        }
    }
}

void OpenFiles::FillFdsaninfo(OpenFilesList &list, pid_t nsPid, uint64_t fdTableAddr)
{
    constexpr size_t fds = sizeof(FdTable::entries) / sizeof(*FdTable::entries);
    size_t entryOffset = offsetof(FdTable, entries);
    uint64_t addr = fdTableAddr + entryOffset;
    FdEntry entrys[fds];
    if (ReadProcMemByPid(nsPid, addr, entrys, sizeof(FdEntry) * fds) != sizeof(FdEntry) * fds) {
        DFXLOGE("[%{public}d]: read nsPid mem error %{public}s", __LINE__, strerror(errno));
        return;
    }
    for (size_t i = 0; i < fds; i++) {
        if (entrys[i].close_tag) {
            list[i].fdsanOwner = entrys[i].close_tag;
        }
    }

    size_t overflowOffset = offsetof(FdTable, overflow);
    uintptr_t overflow = 0;
    uint64_t tmp = fdTableAddr + overflowOffset;
    if (ReadProcMemByPid(nsPid, tmp, &overflow, sizeof(overflow)) != sizeof(overflow)) {
        return;
    }
    if (!overflow) {
        return;
    }

    size_t overflowLength;
    if (ReadProcMemByPid(nsPid, overflow, &overflowLength, sizeof(overflowLength)) != sizeof(overflowLength)) {
        return;
    }
    if (overflowLength > ARG_MAX_NUM) {
        return;
    }

    std::vector<FdEntry> overflowFdEntrys(overflowLength);
    uint64_t address = overflow + offsetof(FdTableOverflow, entries);
    if (ReadProcMemByPid(nsPid, address, overflowFdEntrys.data(), sizeof(FdEntry) * overflowLength) !=
        sizeof(FdEntry) * overflowLength) {
        DFXLOGE("[%{public}d]: read nsPid mem error %{public}s", __LINE__, strerror(errno));
        return;
    }
    size_t fdIndex = fds;
    for (size_t i = 0; i < overflowLength; i++) {
        if (overflowFdEntrys[i].close_tag) {
            list[fdIndex].fdsanOwner = overflowFdEntrys[i].close_tag;
        }
        fdIndex++;
    }
}

std::string OpenFiles::DumpOpenFiles(OpenFilesList files)
{
    std::string openFilesStr = "OpenFiles:\n";
    for (const auto &filePath: files) {
        const std::string path = filePath.second.path;
        uint64_t tag = filePath.second.fdsanOwner;
        const char* type = fdsan_get_tag_type(tag);
        uint64_t val = fdsan_get_tag_value(tag);
        if (!path.empty()) {
            openFilesStr += std::to_string(filePath.first) + "->" + path + " " + type + " " +
                std::to_string(val) + "\n";
        } else {
            openFilesStr += "OpenFilesList contain an entry (fd " +
                std::to_string(filePath.first) + ") with no path or owner\n";
        }
    }
    return openFilesStr;
}
}
}