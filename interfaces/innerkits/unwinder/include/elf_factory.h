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

#ifndef ELF_FACTORY_H
#define ELF_FACTORY_H

#include <memory>
#include <mutex>
#include "dfx_elf.h"
#include "dfx_map.h"
#if defined(ENABLE_MINIDEBUGINFO)
#include "7zCrc.h"
#include "Xz.h"
#include "XzCrc64.h"
#endif

namespace OHOS {
namespace HiviewDFX {
class ElfFactory {
public:
    ElfFactory() = default;
    virtual ~ElfFactory() = default;
    virtual std::shared_ptr<DfxElf> Create() = 0;
};

class RegularElfFactory : public ElfFactory {
public:
    explicit RegularElfFactory(const std::string& filePath) : filePath_(filePath)  { }
    ~RegularElfFactory() = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    RegularElfFactory() = delete;
    std::string filePath_;
};

#if defined(ENABLE_MINIDEBUGINFO)
class MiniDebugInfoFactory : public ElfFactory {
public:
    explicit MiniDebugInfoFactory(const GnuDebugDataHdr& gnuDebugDataHdr) : gnuDebugDataHdr_(gnuDebugDataHdr) { }
    ~MiniDebugInfoFactory() = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    MiniDebugInfoFactory() = delete;
    static void* XzAlloc(ISzAllocPtr, size_t size);
    static void XzFree(ISzAllocPtr, void *address);
    bool XzDecompress(const uint8_t *src, size_t srcLen, std::vector<uint8_t>& out);
    GnuDebugDataHdr gnuDebugDataHdr_;
};
#endif

class CompressHapElfFactory : public ElfFactory {
public:
    explicit CompressHapElfFactory(const std::string& filePath, std::shared_ptr<DfxMap> prevMap, uint64_t& offset)
        : filePath_(std::move(filePath)), prevMap_(prevMap), offset_(offset) { }
    ~CompressHapElfFactory() = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    CompressHapElfFactory() = delete;
    bool VerifyElf(int fd, size_t& elfSize);
    const std::string filePath_;
    std::shared_ptr<DfxMap> prevMap_;
    uint64_t& offset_;
};

class VdsoElfFactory : public ElfFactory {
public:
    explicit VdsoElfFactory(uint64_t begin, size_t size, pid_t pid) : begin_(begin), size_(size), pid_(pid){ }
    ~VdsoElfFactory() = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    VdsoElfFactory() = delete;
    uint64_t begin_;
    size_t size_;
    pid_t pid_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
