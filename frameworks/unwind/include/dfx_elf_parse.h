/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef DFX_ELF_PARSE_H
#define DFX_ELF_PARSE_H

#include <cstddef>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "dfx_define.h"
#include "dfx_mmap.h"
#include "elf_define.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
struct ElfLoadInfo {
    uint64_t offset = 0;
    uint64_t tableVaddr = 0;
    size_t tableSize = 0;
};

struct ElfSymbol {
    uint16_t secIndex_;
    uint32_t nameIndex_;
    uint64_t symValue_;
    uint64_t symSize_;
};

class ElfParse {
public:
    ElfParse() = default;
    virtual ~ElfParse() { Clear(); }

    bool Init(const std::string &file);
    void Clear();

    bool InitHeaders();
    bool IsValid();
    uint8_t GetClassType();
    ArchType GetArchType();

    virtual std::string GetElfName();
    virtual int64_t GetLoadBias();
    virtual std::string GetBuildId();
    virtual bool GetFuncNameAndOffset(uint64_t pc, std::string* funcName, uint64_t* start, uint64_t* end);
    template <typename ShdrType>
    bool FindSection(ShdrType& shdr, const std::string secname);

protected:
    bool Read(uint64_t pos, void *buf, size_t size);
    size_t Size();

    bool ParseElfIdent();
    template <typename EhdrType, typename PhdrType, typename ShdrType>
    bool ParseAllHeaders();
    template <typename EhdrType>
    bool ParseElfHeaders(const EhdrType& ehdr);
    template <typename EhdrType, typename PhdrType>
    bool ParseProgramHeaders(const EhdrType& ehdr);
    template <typename EhdrType, typename ShdrType>
    bool ParseSectionHeaders(const EhdrType& ehdr);

private:
    std::shared_ptr<DfxMmap> mmap_;
    bool valid_ = false;
    uint8_t classType_;
    ArchType archType_;
    int64_t loadBias_ = 0;
    MAYBE_UNUSED std::string buildIdHex_;
    MAYBE_UNUSED std::string buildId_;
    std::unordered_map<uint32_t, ElfSymbol> symbols_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
};

class ElfParse32 : public ElfParse {
public:
};

class ElfParse64 : public ElfParse {
public:
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
