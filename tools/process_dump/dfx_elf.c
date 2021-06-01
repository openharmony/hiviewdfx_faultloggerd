/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "dfx_elf.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dfx_define.h"
#include "dfx_log.h"

#define MAX_MAP_SIZE 65536

BOOL InitDfxElfImage(const char *path, DfxElfImage **image)
{
    char realPath[PATH_MAX] = {0};
    if (realpath(path, realPath) == NULL) {
        DfxLogWarn("Fail to do realpath(%s).", path);
        return FALSE;
    }

    *image = (DfxElfImage *)calloc(1, sizeof(DfxElfImage));
    if ((*image) == NULL) {
        DfxLogWarn("Fail to create ElfImage(%s).", path);
        return FALSE;
    }

    (*image)->fd = TEMP_FAILURE_RETRY(open(realPath, O_RDONLY | O_CLOEXEC));
    if ((*image)->fd < 0) {
        DfxLogWarn("Fail to open elf file(%s).", realPath);
        return FALSE;
    }

    struct stat elfStat;
    if (fstat((*image)->fd, &elfStat) != 0) {
        DfxLogWarn("Fail to get elf size.");
        close((*image)->fd);
        (*image)->fd = -1;
        return FALSE;
    }

    (*image)->size = (uint64_t)elfStat.st_size;
    if (!ParseElfHeader((*image))) {
        DfxLogWarn("Fail to parse elf header.");
        close((*image)->fd);
        (*image)->fd = -1;
        return FALSE;
    }

    if (!ParseElfProgramHeader((*image))) {
        DfxLogWarn("Fail to parse elf program header.");
        close((*image)->fd);
        (*image)->fd = -1;
        return FALSE;
    }

    close((*image)->fd);
    (*image)->fd = -1;
    return TRUE;
}

BOOL ParseElfHeader(DfxElfImage *image)
{
    ssize_t nread = TEMP_FAILURE_RETRY(read(image->fd, &(image->header), sizeof(image->header)));
    if (nread < 0 || nread != sizeof(image->header)) {
        DfxLogWarn("Failed to read elf header.");
        return FALSE;
    }
    return TRUE;
}

BOOL ParseElfProgramHeader(DfxElfImage *image)
{
    size_t size = image->header.e_phnum * sizeof(ElfW(Phdr));
    if (size > MAX_MAP_SIZE) {
        DfxLogWarn("Exceed max mmap size.");
        return FALSE;
    }

    size_t offset = image->header.e_phoff;
    size_t startOffset = offset & (size_t)(getpagesize() - 1);
    size_t alignedOffset = offset & (~(size_t)(getpagesize() - 1));
    uint64_t endOffset;
    if (__builtin_add_overflow((uint64_t)size, (uint64_t)offset, &endOffset) ||
        __builtin_add_overflow((uint64_t)endOffset, (uint64_t)startOffset, &endOffset)) {
        DfxLogWarn("Offset calculate error.");
        return FALSE;
    }

    size_t mapSize = (size_t)(endOffset - offset);
    if (mapSize > MAX_MAP_SIZE) {
        DfxLogWarn("Exceed max mmap size.");
        return FALSE;
    }

    void *map = mmap(NULL, mapSize, PROT_READ, MAP_PRIVATE, image->fd, (off_t)alignedOffset);
    if (map == MAP_FAILED) {
        DfxLogWarn("Failed to mmap elf.");
        return FALSE;
    }

    ElfW(Phdr) *phdrTable = (ElfW(Phdr) *)((uint8_t *)map + startOffset);
    for (size_t i = 0; i < image->header.e_phnum; i++) {
        ElfW(Phdr) *phdr = &(phdrTable[i]);
        if (((phdr->p_type != PT_LOAD) && !(phdr->p_flags & PF_X))) {
            continue;
        }
        image->loadBias = (size_t)(phdr->p_vaddr - phdr->p_offset);
        break;
    }
    munmap(map, mapSize);
    return TRUE;
}

void DestroyDfxElfImage(DfxElfImage *image)
{
    if (image->fd >= 0) {
        close(image->fd);
        image->fd = -1;
    }
}
