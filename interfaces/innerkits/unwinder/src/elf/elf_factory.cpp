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

#include "elf_factory.h"

#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <securec.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#if defined(ENABLE_MINIDEBUGINFO)
#include "unwinder_config.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "ElfFactory"
#define EXPAND_FACTOR 2
}
#if defined(ENABLE_MINIDEBUGINFO)
std::shared_ptr<DfxElf> MiniDebugInfoFactory::Create()
{
    DFX_TRACE_SCOPED_DLSYM("CreateMiniDebugInfo");
    if (!UnwinderConfig::GetEnableMiniDebugInfo()) {
        return nullptr;
    }

    std::vector<uint8_t> miniDebugInfoData;
    if (!XzDecompress(gnuDebugDataHdr_.address, gnuDebugDataHdr_.size, miniDebugInfoData)) {
        DFXLOGE("Failed to decompressed .gnu_debugdata seciton.");
        return nullptr;
    }
    // miniDebugInfoData store the decompressed bytes.
    // use these bytes to construct an elf.
    auto mMap = std::make_shared<DfxMmap>();
    if (!mMap->Init(std::move(miniDebugInfoData))) {
        DFXLOGE("Failed to init mmap_!");
        return nullptr;
    }
    auto miniDebugInfo = std::make_shared<DfxElf>(mMap);
    if (!miniDebugInfo->IsValid()) {
        DFXLOGE("Failed to parse mini debuginfo Elf.");
        return nullptr;
    }
    return miniDebugInfo;
}

void* MiniDebugInfoFactory::XzAlloc(ISzAllocPtr, size_t size)
{
    return malloc(size);
}

void MiniDebugInfoFactory::XzFree(ISzAllocPtr, void *address)
{
    free(address);
}

bool MiniDebugInfoFactory::XzDecompress(const uint8_t *src, size_t srcLen, std::vector<uint8_t>& out)
{
    DFX_TRACE_SCOPED_DLSYM("XzDecompress");
    ISzAlloc alloc;
    CXzUnpacker state;
    alloc.Alloc = XzAlloc;
    alloc.Free = XzFree;
    XzUnpacker_Construct(&state, &alloc);
    CrcGenerateTable();
    Crc64GenerateTable();
    size_t srcOff = 0;
    size_t dstOff = 0;
    out.resize(srcLen);
    ECoderStatus status = CODER_STATUS_NOT_FINISHED;
    while (status == CODER_STATUS_NOT_FINISHED) {
        out.resize(out.size() * EXPAND_FACTOR);
        size_t srcRemain = srcLen - srcOff;
        size_t dstRemain = out.size() - dstOff;
        int res = XzUnpacker_Code(&state,
                                  reinterpret_cast<Byte*>(&out[dstOff]), &dstRemain,
                                  reinterpret_cast<const Byte*>(&src[srcOff]), &srcRemain,
                                  true, CODER_FINISH_ANY, &status);
        if (res != SZ_OK) {
            XzUnpacker_Free(&state);
            return false;
        }
        srcOff += srcRemain;
        dstOff += dstRemain;
    }
    XzUnpacker_Free(&state);
    if (!XzUnpacker_IsStreamWasFinished(&state)) {
        return false;
    }
    out.resize(dstOff);
    return true;
}
#endif

std::shared_ptr<DfxElf> RegularElfFactory::Create()
{
    std::shared_ptr<DfxElf> regularElf = std::make_shared<DfxElf>();
    if (filePath_.empty()) {
        DFXLOGE("file path is empty!");
        return regularElf;
    }
    DFXLOGU("file: %{public}s", filePath_.c_str());
#if defined(is_ohos) && is_ohos
    if (!DfxMaps::IsLegalMapItem(filePath_)) {
        DFXLOGD("Illegal map file, please check file: %{public}s", filePath_.c_str());
        return regularElf;
    }
#endif
    std::string realPath = filePath_;
    if (!StartsWith(filePath_, "/proc/")) { // sandbox file should not be check by realpath function
        if (!RealPath(filePath_, realPath)) {
            DFXLOGW("Failed to realpath %{public}s, errno(%{public}d)", filePath_.c_str(), errno);
            return regularElf;
        }
    }
#if defined(is_mingw) && is_mingw
    int fd = OHOS_TEMP_FAILURE_RETRY(open(realPath.c_str(), O_RDONLY | O_BINARY));
#else
    int fd = OHOS_TEMP_FAILURE_RETRY(open(realPath.c_str(), O_RDONLY));
#endif
    if (fd <= 0) {
        DFXLOGE("Failed to open file: %{public}s, errno(%d)", filePath_.c_str(), errno);
        return regularElf;
    }
    do {
        auto size = static_cast<size_t>(GetFileSize(fd));
        auto mMap = std::make_shared<DfxMmap>();
        if (!mMap->Init(fd, size, 0)) {
            DFXLOGE("Failed to mmap init.");
            break;
        }
        regularElf->SetMmap(mMap);
    } while (false);
    close(fd);
    return regularElf;
}

std::shared_ptr<DfxElf> VdsoElfFactory::Create()
{
#if is_ohos && !is_mingw
    std::vector<uint8_t> shmmData(size_);
    size_t byte = ReadProcMemByPid(pid_, begin_, shmmData.data(), size_);
    if (byte != size_) {
        DFXLOGE("Failed to read shmm data");
        return nullptr;
    }

    auto mMap = std::make_shared<DfxMmap>();
    if (!mMap->Init(std::move(shmmData))) {
        DFXLOGE("Failed to init mmap_!");
        return nullptr;
    }
    auto vdsoElf = std::make_shared<DfxElf>(mMap);
    if (!vdsoElf->IsValid()) {
        DFXLOGE("Failed to parse Embedded Elf.");
        return nullptr;
    }
    return vdsoElf;
#endif
    return nullptr;
}


std::shared_ptr<DfxElf> CompressHapElfFactory::Create()
{
    /*
      elf header is in the first mmap area
      c3840000-c38a6000 r--p 00174000 /data/storage/el1/bundle/entry.hap <- program header
      c38a6000-c3945000 r-xp 001d9000 /data/storage/el1/bundle/entry.hap <- pc is in this region
      c3945000-c394b000 r--p 00277000 /data/storage/el1/bundle/entry.hap
      c394b000-c394c000 rw-p 0027c000 /data/storage/el1/bundle/entry.hap
    */
    if (prevMap_ == nullptr) {
        DFXLOGE("current hap map item or prev map item , maybe pc is wrong?");
        return nullptr;
    }
    if (!StartsWith(filePath_, "/proc") || !EndsWith(filePath_, ".hap")) {
        DFXLOGD("Illegal file path, please check file: %{public}s", filePath_.c_str());
        return nullptr;
    }
    int fd = OHOS_TEMP_FAILURE_RETRY(open(filePath_.c_str(), O_RDONLY));
    if (fd < 0) {
        DFXLOGE("Failed to open hap file, errno(%{public}d)", errno);
        return nullptr;
    }
    std::shared_ptr<DfxElf> compressHapElf = nullptr;
    do {
        size_t elfSize = 0;
        if (!VerifyElf(fd, elfSize)) {
            break;
        }
        offset_ -= prevMap_->offset;
        DFXLOGU("elfSize: %{public}zu", elfSize);
        auto mMap = std::make_shared<DfxMmap>();
        if (!mMap->Init(fd, elfSize, prevMap_->offset)) {
            DFXLOGE("Failed to init mmap_!");
            break;
        }
        compressHapElf = std::make_shared<DfxElf>(mMap);
        if (!compressHapElf->IsValid()) {
            DFXLOGE("Failed to parse compress hap Elf.");
            compressHapElf = nullptr;
            break;
        }
    } while (false);
    close(fd);
    return compressHapElf;
}

bool CompressHapElfFactory::VerifyElf(int fd, size_t& elfSize)
{
    size_t size = prevMap_->end - prevMap_->begin;
    auto mmap = std::make_shared<DfxMmap>();
    if (!mmap->Init(fd, size, static_cast<off_t>(prevMap_->offset))) {
        DFXLOGE("Failed to mmap program header in hap.");
        return false;
    }
    elfSize = 0;
    const uint8_t* data = static_cast<const uint8_t*>(mmap->Get());
    if (memcmp(data, ELFMAG, SELFMAG) != 0) {
        DFXLOGD("Invalid elf hdr?");
        return false;
    }
    uint8_t classType = data[EI_CLASS];
    if (classType == ELFCLASS32) {
        const Elf32_Ehdr* ehdr = reinterpret_cast<const Elf32_Ehdr *>(data);
        elfSize = static_cast<size_t>(ehdr->e_shoff + (ehdr->e_shentsize * ehdr->e_shnum));
    } else if (classType == ELFCLASS64) {
        const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr *>(data);
        elfSize = static_cast<size_t>(ehdr->e_shoff + (ehdr->e_shentsize * ehdr->e_shnum));
    }
    auto fileSize = GetFileSize(fd);
    if (elfSize <= 0 || elfSize + prevMap_->offset > static_cast<uint64_t>(fileSize)) {
        DFXLOGE("Invalid elf size? elf size: %{public}d, hap size: %{public}d", (int)elfSize, (int)fileSize);
        elfSize = 0;
        return false;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
