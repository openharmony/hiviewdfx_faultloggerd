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
#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_map.h"

namespace OHOS {
namespace HiviewDFX {
class ElfFactory {
public:
    virtual ~ElfFactory() = default;
    virtual std::shared_ptr<DfxElf> Create() = 0;
};

class RegularElfFactory : public ElfFactory {
public:
    explicit RegularElfFactory(const std::string& filePath) : filePath_(filePath)  { }
    ~RegularElfFactory() override = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    std::string filePath_ = "";
};

#if defined(ENABLE_MINIDEBUGINFO)
class MiniDebugInfoFactory : public ElfFactory {
public:
    explicit MiniDebugInfoFactory(const GnuDebugDataHdr& gnuDebugDataHdr) : gnuDebugDataHdr_(gnuDebugDataHdr) { }
    ~MiniDebugInfoFactory() override = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    bool XzDecompress(const uint8_t *src, size_t srcLen, std::vector<uint8_t>& out);
    GnuDebugDataHdr gnuDebugDataHdr_ {};
};
#endif

class CompressHapElfFactory : public ElfFactory {
public:
    explicit CompressHapElfFactory(const std::string& filePath, std::shared_ptr<DfxMap> prevMap, uint64_t& offset)
        : filePath_(std::move(filePath)), prevMap_(prevMap), offset_(offset) { }
    ~CompressHapElfFactory() override = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    bool VerifyElf(int fd, size_t& elfSize);
    const std::string filePath_ = "";
    std::shared_ptr<DfxMap> prevMap_ = nullptr;
    uint64_t& offset_;
};

class VdsoElfFactory : public ElfFactory {
public:
    explicit VdsoElfFactory(uint64_t begin, size_t size, pid_t pid) : begin_(begin), size_(size), pid_(pid){ }
    ~VdsoElfFactory() override = default;
    std::shared_ptr<DfxElf> Create() override;
private:
    VdsoElfFactory() = delete;
    MAYBE_UNUSED uint64_t begin_ = 0;
    MAYBE_UNUSED size_t size_ = 0;
    MAYBE_UNUSED pid_t pid_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
