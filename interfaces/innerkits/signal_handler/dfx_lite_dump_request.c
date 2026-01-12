/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dfx_lite_dump_request.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <securec.h>
#include <signal.h>
#include <sigchain.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <info/fatal_message.h>
#include <sys/syscall.h>

#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_log.h"
#include "dfx_signalhandler_exception.h"
#include "dfx_allocator.h"
#include "faultlog_client.h"
#include "safe_reader.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSignalHandler"
#endif

#define FD_TABLE_SIZE 128
#define ARG_MAX_NUM 131072

typedef struct FdEntry {
    _Atomic(uint64_t) close_tag;
    _Atomic(char) signal_flag;
} FdEntry;

typedef struct FdTableOverflow {
    size_t len;
    struct FdEntry entries[];
} FdTableOverflow;

typedef struct FdTable {
    _Atomic(enum fdsan_error_level) error_level;
    struct FdEntry entries[FD_TABLE_SIZE];
    _Atomic(struct FdTableOverflow*) overflow;
} FdTable;

static void* g_mmapSpace = MAP_FAILED;
static unsigned int g_mmapPos = 0;

static const int TOTAL_MEMORY_SIZE = PRIV_COPY_STACK_BUFFER_SIZE + PROC_STAT_BUF_SIZE + PROC_STATM_BUF_SIZE;
static const int FILE_PATH_LEN = 256;
static const char * const PID_STR_NAME = "Pid:";
static const char * const THREAD_SELF_STATUS_PATH = "/proc/thread-self/status";

pid_t GetProcId(const char *statusPath, const char *item)
{
    pid_t pid = -1;
    if (statusPath == NULL || item == NULL) {
        return pid;
    }

    int fd = OHOS_TEMP_FAILURE_RETRY(open(statusPath, O_RDONLY));
    if (fd < 0) {
        DFXLOGE("GetRealPid:: open failed! pid:%{public}d, errno:%{public}d).", pid, errno);
        return pid;
    }

    char buf[LINE_BUF_SIZE] = {0};
    int i = 0;
    char b;
    ssize_t nRead = 0;
    while (nRead >= 0) {
        nRead = OHOS_TEMP_FAILURE_RETRY(read(fd, &b, sizeof(char)));
        if (nRead <= 0 || b == '\0') {
            DFXLOGE("GetRealPid:: read failed! pid:(%{public}d), errno:(%{public}d), nRead(%{public}zd), \
                readchar(%{public}02X).", pid, errno, nRead, b);
            break;
        }

        if (b == '\n' || i == LINE_BUF_SIZE) {
            if (strncmp(buf, item, strlen(item)) != 0) {
                i = 0;
                (void)memset_s(buf, sizeof(buf), '\0', sizeof(buf));
                continue;
            }
            if (sscanf_s(buf, "%*[^0-9]%d", &pid) != 1) {
                DFXLOGE("GetRealPid sscanf failed! pid:%{public}d, err:%{public}d, buf%{public}s.", pid, errno, buf);
            }
            break;
        }
        buf[i] = b;
        i++;
    }
    close(fd);
    return pid;
}

bool MMapMemoryOnce()
{
    DFXLOGI("lite dump start mmap memory");
    g_mmapSpace = mmap(NULL, TOTAL_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_mmapSpace == MAP_FAILED) {
        DFXLOGE("lite dump mmap failed %{public}d", errno);
        return false;
    }
    DFXLOGI("lite dump finish mmap memory");
    return true;
}

void UnmapMemoryOnce()
{
    if (g_mmapSpace == MAP_FAILED) {
        return;
    }
    DFXLOGI("lite dump start unmap memory");
    if (munmap(g_mmapSpace, TOTAL_MEMORY_SIZE) == -1) {
        DFXLOGE("lite dump munmap failed %{public}d", errno);
    } else {
        DFXLOGI("lite dump finish unmap memory");
    }
    g_mmapSpace = MAP_FAILED;
}

/**
 * should collect stack in src process
 */
bool CollectStack(const struct ProcessDumpRequest *request)
{
    DFXLOGI("start collect process stack");
    if (g_mmapSpace == MAP_FAILED) {
        DFXLOGE("mmap failed");
        return false;
    }
#if defined(__aarch64__)
    if (g_mmapPos + PRIV_COPY_STACK_BUFFER_SIZE > TOTAL_MEMORY_SIZE) {
        DFXLOGE("collect statck mmap space is over flow");
        return false;
    }
    char* destPtr = (char*)g_mmapSpace + g_mmapPos;
    uintptr_t srcPtr =  ((ucontext_t)request->context).uc_mcontext.sp - PRIV_STACK_FORWARD_BUF_SIZE;
    CopyReadableBufSafe((uintptr_t)destPtr, PRIV_COPY_STACK_BUFFER_SIZE, srcPtr, PRIV_COPY_STACK_BUFFER_SIZE);
    DeInitPipe();
#endif
    g_mmapPos += PRIV_COPY_STACK_BUFFER_SIZE;
    DFXLOGI("finish collect process stack");
    return true;
}

bool CollectStat(const struct ProcessDumpRequest *request)
{
    if (g_mmapSpace == MAP_FAILED) {
        DFXLOGE("mmap failed");
        return false;
    }
    if (g_mmapPos + PROC_STAT_BUF_SIZE > TOTAL_MEMORY_SIZE) {
        DFXLOGE("collect stat memory size over flow");
        return false;
    }

    char path[FILE_PATH_LEN];
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/stat", request->pid);
    if (ret < 0) {
        DFXLOGE("CollectStat :: snprintf_s failed, ret(%{public}d)", ret);
        g_mmapPos += PROC_STAT_BUF_SIZE;
        return false;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        DFXLOGI("failed open %{public}s errno %{public}d", path, errno);
        g_mmapPos += PROC_STAT_BUF_SIZE;
        return false;
    }

    char* stat = (char*)g_mmapSpace + g_mmapPos;
    stat[PROC_STAT_BUF_SIZE - 1] = '\0';
    ssize_t n = read(fd, stat, PROC_STAT_BUF_SIZE - 1);
    g_mmapPos += PROC_STAT_BUF_SIZE;
    if (n > 0) {
        stat[n] = '\0';
    }
    DFXLOGI("finish collect proc stat");
    close(fd);
    return true;
}

bool CollectStatm(const struct ProcessDumpRequest *request)
{
    if (g_mmapSpace == MAP_FAILED) {
        DFXLOGE("mmap failed");
        return false;
    }
    if (g_mmapPos + PROC_STATM_BUF_SIZE > TOTAL_MEMORY_SIZE) {
        DFXLOGE("collect statm mmap space is over flow");
        return false;
    }
    char path[FILE_PATH_LEN];
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/statm", request->pid);
    if (ret < 0) {
        DFXLOGE("CollectStatm :: snprintf_s failed, ret(%{public}d)", ret);
        g_mmapPos += PROC_STATM_BUF_SIZE;
        return false;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        DFXLOGI("failed to open %{public}s errno %{public}d", path, errno);
        g_mmapPos += PROC_STATM_BUF_SIZE;
        return false;
    }

    char* statm = (char*)g_mmapSpace + g_mmapPos;
    statm[PROC_STAT_BUF_SIZE - 1] = '\0';

    ssize_t n = read(fd, statm, PROC_STATM_BUF_SIZE - 1);
    g_mmapPos += PROC_STATM_BUF_SIZE;
    if (n > 0) {
        statm[n] = '\0';
    }
    DFXLOGI("finish collect proc stam");
    close(fd);
    return true;
}

typedef struct {
    int regIndex;
    bool isPcLr;
    uintptr_t regAddr;
} RegInfo;

NO_SANITIZE bool CreateMemoryBlock(const int fd, const RegInfo info, int regIdx)
{
    const size_t size = sizeof(uintptr_t);
    size_t count = COMMON_REG_MEM_SIZE;
    uintptr_t forwardSize = COMMON_REG_MEM_FORWARD_SIZE;
    if (info.isPcLr) {
        forwardSize = SPECIAL_REG_MEM_FORWARD_SIZE;
        count = SPECIAL_REG_MEM_SIZE;
    }

    size_t mmapSize = count * size;
    void* mptr = mmap(NULL, mmapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); //
    if (mptr == MAP_FAILED) {
        DFXLOGE("mmap failed! %{public}d", errno);
        return false;
    }

    uintptr_t targetAddr = info.regAddr;
    targetAddr = targetAddr & ~(size - 1);
    targetAddr -= (forwardSize * size);
    char *p = (char*)mptr;
    (void)memset_s(p, mmapSize, -1, mmapSize);

    CopyReadableBufSafe((uintptr_t)mptr, mmapSize, targetAddr, mmapSize);
    write(fd, mptr, mmapSize);
    munmap(mptr, mmapSize);
    return true;
}

bool CollectMemoryNearRegisters(int fd, ucontext_t *context)
{
    char start[50] = "start trans register";
    write(fd, start, sizeof(start));
#if defined(__aarch64__)
    const uintptr_t pacMaskDefault = ~(uintptr_t)0xFFFFFF8000000000;
    int lrIndex = 30;
    for (uint16_t i = 0; i < lrIndex; i++) {
        RegInfo info = {i, false, context->uc_mcontext.regs[i] & pacMaskDefault};
        CreateMemoryBlock(fd, info, i);
    }

    RegInfo info = {lrIndex, true, context->uc_mcontext.regs[lrIndex] & pacMaskDefault};
    CreateMemoryBlock(fd, info, lrIndex);

    int spIndex = 31;
    info.regIndex = spIndex;
    info.isPcLr = false;
    info.regAddr = (context->uc_mcontext.sp & pacMaskDefault);
    CreateMemoryBlock(fd, info, spIndex);

    int pcIndex = 32;
    info.regIndex = pcIndex;
    info.isPcLr = true;
    info.regAddr = (context->uc_mcontext.pc & pacMaskDefault);
    CreateMemoryBlock(fd, info, pcIndex);
#endif
    char end[50] = "end trans register";
    write(fd, end, sizeof(end));
    return true;
}

bool CollectMaps(const int pipeFd, const char* path)
{
    if (path == NULL || pipeFd < 0) {
        DFXLOGI("%{public}s path or pipeFd is invalid", __func__);
        return false;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        DFXLOGI("open %{public}s failed, errno %{public}d", path, errno);
        return false;
    }

    char buf[LINE_BUF_SIZE];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        OHOS_TEMP_FAILURE_RETRY(write(pipeFd, buf, n));
    }

    close(fd);
    return true;
}

typedef struct FdTableEntry {
    struct FdEntry *entries;       // mmap分配的条目数组
    size_t entryCount;             // 当前条目数
} __attribute__((packed)) FdTableEntry;

static void FillFdsaninfo(FdTableEntry *fdEntries, FdTableEntry *overflowEntries, const uint64_t fdTableAddr)
{
    size_t entryOffset = offsetof(FdTable, entries);
    uint64_t addr = fdTableAddr + entryOffset;

    fdEntries->entryCount = FD_TABLE_SIZE;
    size_t mmapSize = FD_TABLE_SIZE * sizeof(FdEntry);
    fdEntries->entries = mmap(NULL, mmapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fdEntries->entries == MAP_FAILED) {
        DFXLOGE("mmap fdEntries failed!");
        return;
    }

    if (CopyReadableBufSafe((uintptr_t)fdEntries->entries, mmapSize, addr, mmapSize) != mmapSize) {
        DFXLOGE("%{public}s read FdEntry error %{public}d", __func__, errno);
        return;
    }

    size_t overflowOffset = offsetof(FdTable, overflow);
    uintptr_t overflow = 0;
    addr = fdTableAddr + overflowOffset;
    mmapSize = sizeof(overflow);
    if ((CopyReadableBufSafe((uintptr_t)&overflow, mmapSize, addr, mmapSize) != mmapSize) || (overflow == 0)) {
        DFXLOGE("read overflow error %{public}d", errno);
        return;
    }

    size_t overflowLength;
    mmapSize = sizeof(overflowLength);
    if (CopyReadableBufSafe((uintptr_t)&overflowLength, mmapSize, overflow, mmapSize) != mmapSize) {
        DFXLOGE("%{public}s read overflowLength error %{public}d", __func__, errno);
        return;
    }
    if (overflowLength > ARG_MAX_NUM) {
        return;
    }

    DFXLOGI("%{public}s overflow length %{public}zu", __func__, overflowLength);
    overflowEntries->entryCount = overflowLength;
    mmapSize = overflowEntries->entryCount * sizeof(FdEntry);
    overflowEntries->entries = mmap(NULL, mmapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (overflowEntries->entries == MAP_FAILED) {
        DFXLOGE("mmap overflowEntries failed!");
        return;
    }

    addr = overflow + offsetof(FdTableOverflow, entries);
    if (CopyReadableBufSafe((uintptr_t)overflowEntries->entries, mmapSize, addr, mmapSize) != mmapSize) {
        DFXLOGE("%{public}s read FdTableOverflow error %{public}d", __func__, errno);
    }
}

static void FillFdsanOwner(FdTableEntry fdEntries, FdTableEntry overflowEntries, uint64_t* fdsanOwners)
{
    if (fdEntries.entries == MAP_FAILED) {
        return;
    }
    for (size_t i = 0; i < fdEntries.entryCount; i++) {
        struct FdEntry *entry = &(fdEntries.entries[i]);
        if (entry->close_tag) {
            fdsanOwners[i] = entry->close_tag;
        }
    }
    munmap(fdEntries.entries, fdEntries.entryCount * sizeof(FdEntry));

    if (overflowEntries.entries == MAP_FAILED) {
        return;
    }
    for (size_t j = 0; j < overflowEntries.entryCount; j++) {
        struct FdEntry *entry = &(overflowEntries.entries[j]);
        if (entry->close_tag) {
            fdsanOwners[FD_TABLE_SIZE + j] = entry->close_tag;
        }
    }
    munmap(overflowEntries.entries, overflowEntries.entryCount * sizeof(FdEntry));
}

void WriteFileItems(int pipeWriteFd, DIR *dir, const char * path, const uint64_t* fdsanOwners, int fdMaxIndex)
{
    struct dirent *entry;
    char target[FILE_PATH_LEN];
    char linkpath[FILE_PATH_LEN];
    ssize_t len;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        int ret = snprintf_s(linkpath, sizeof(linkpath), sizeof(linkpath) - 1, "%s/%s", path, entry->d_name);
        if (ret < 0) {
            DFXLOGE("CollectOpenFiles :: snprintf_s failed, ret(%{public}d)", ret);
            continue;
        }
        len = readlink(linkpath, target, sizeof(target) - 1);
        if (len != -1) {
            target[len] = '\0';
            long fd;
            if (!SafeStrtol(entry->d_name, &fd, DECIMAL_BASE)) {
                continue;
            }
            if (fd < 0 || fd >= fdMaxIndex) {
                continue;
            }
            uint64_t tag = fdsanOwners[fd];
            const char* type = fdsan_get_tag_type(tag);
            uint64_t val = fdsan_get_tag_value(tag);
            char output[512]; // 512 : output
            ret = snprintf_s(output, sizeof(output), sizeof(output) - 1, "%d->%s %s %lu\n", fd, target, type, val);
            if (ret < 0) {
                DFXLOGE("CollectOpenFiles :: snprintf_s failed, ret(%{public}d)", ret);
            }
            write(pipeWriteFd, output, strlen(output));
        } else {
            DFXLOGE("fd %{public}s -> (unreadable: %{public}d)\n", entry->d_name, errno);
        }
    }
}

bool CollectOpenFiles(int pipeWriteFd, const uint64_t fdTableAddr)
{
    char path[FILE_PATH_LEN];
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/fd",
        GetProcId(PROC_SELF_STATUS_PATH, PID_STR_NAME));
    if (ret < 0) {
        DFXLOGE("CollectOpenFiles :: snprintf_s failed, ret(%{public}d)", ret);
        return false;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        DFXLOGI("open files failed to open dir %{public}d", errno);
        return false;
    }

    char openFiles[] = "OpenFiles:\n";
    write(pipeWriteFd, openFiles, strlen(openFiles));

    FdTableEntry fdEntries = {0};
    FdTableEntry overflowEntries = {0};
    FillFdsaninfo(&fdEntries, &overflowEntries, fdTableAddr);
    size_t entryCount = fdEntries.entryCount + overflowEntries.entryCount;

    size_t mmapSize = entryCount * sizeof(uint64_t);
    uint64_t* fdsanOwners = mmap(NULL, mmapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fdsanOwners == MAP_FAILED) {
        closedir(dir);
        DFXLOGE("mmap fdsanOwners failed!");
        return false;
    }
    FillFdsanOwner(fdEntries, overflowEntries, fdsanOwners);
    WriteFileItems(pipeWriteFd, dir, path, fdsanOwners, (int)entryCount);
    closedir(dir);
    munmap(fdsanOwners, mmapSize);
    return true;
}

bool LiteCrashHandler(struct ProcessDumpRequest *request)
{
    DFXLOGI("start enter %{public}s", __func__);
    RegisterAllocator();
    RequestLimitedProcessDump(request->pid);
    int pipeWriteFd = -1;
    RequestLimitedPipeFd(PIPE_WRITE, &pipeWriteFd, request->pid, request->processName);
    if (pipeWriteFd < 0) {
        DFXLOGE("lite dump failed to request pipe %{public}d", errno);
        return false;
    }

    if (write(pipeWriteFd, request, sizeof(struct ProcessDumpRequest)) != sizeof(struct ProcessDumpRequest)) {
        DFXLOGE("failed to write dump request %{public}d", errno);
        close(pipeWriteFd);
        UnregisterAllocator();
        return false;
    }

    if (write(pipeWriteFd, g_mmapSpace, TOTAL_MEMORY_SIZE) != TOTAL_MEMORY_SIZE) {
        DFXLOGE("failed to write mmap buf %{public}d", errno);
        close(pipeWriteFd);
        UnregisterAllocator();
        return true;
    }

    CollectMemoryNearRegisters(pipeWriteFd, &request->context);
    CollectMaps(pipeWriteFd, PROC_SELF_MAPS_PATH);
    CollectOpenFiles(pipeWriteFd, (uint64_t)fdsan_get_fd_table());

    close(pipeWriteFd);
    UnregisterAllocator();
    DFXLOGI("finish %{public}s", __func__);
    return true;
}

void UpdateSanBoxProcess(struct ProcessDumpRequest *request)
{
    if (request == NULL) {
        return;
    }
    request->pid = GetProcId(PROC_SELF_STATUS_PATH, PID_STR_NAME);
    request->tid = GetProcId(THREAD_SELF_STATUS_PATH, PID_STR_NAME);
    GetThreadNameByTid(request->tid, request->threadName, sizeof(request->threadName));
}

void ResetLiteDump(void)
{
    g_mmapPos = 0;
}
