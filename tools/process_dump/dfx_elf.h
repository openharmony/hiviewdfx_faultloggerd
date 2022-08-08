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
#ifndef DFX_ELF_IMAGE_H
#define DFX_ELF_IMAGE_H
#include <cstddef>
#include <elf.h>
#include <link.h>
#include <memory>
#include <string>
#include <vector>
#include "iosfwd"

namespace OHOS {
namespace HiviewDFX {
struct ElfLoadInfo {
    uint64_t vaddr;
    uint64_t offset;
};
using ElfLoadInfo = struct ElfLoadInfo;

class DfxElf {
public:
    DfxElf() = default;
    ~DfxElf() = default;

    static std::shared_ptr<DfxElf> Create(const std::string path);
    bool ParseElfHeader();
    bool ParseElfProgramHeader();
    uint64_t FindRealLoadOffset(uint64_t offset) const;
    void CreateLoadInfo(uint64_t vaddr, uint64_t offset);

    std::string GetName() const;
    void SetName(const std::string &name);
    std::string GetPath() const;
    void SetPath(const std::string &path);
    int32_t GetFd() const;
    void SetFd(int32_t fdValue);
    size_t GetLoadBias() const;
    void SetLoadBias(size_t loadBias);
    uint64_t GetSize() const;
    void SetSize(uint64_t size);
    ElfW(Ehdr) GetHeader() const;
    void SetHeader(ElfW(Ehdr) header);
    std::vector<ElfLoadInfo> GetInfos() const;
    void SetInfos(const std::vector<ElfLoadInfo> &infos);

private:
    std::string name_;
    std::string path_;
    int32_t fd_;
    size_t loadBias_;
    uint64_t size_;
    ElfW(Ehdr)header_;
    std::vector<ElfLoadInfo> infos_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
