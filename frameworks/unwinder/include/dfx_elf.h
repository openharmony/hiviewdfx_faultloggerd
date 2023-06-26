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
#ifndef DFX_ELF_H
#define DFX_ELF_H

#include <cstddef>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <memory>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "unwinder_define.h"
#include "dfx_memory.h"
#include "dfx_symbols.h"

namespace OHOS {
namespace HiviewDFX {
struct ElfFileInfo {
    std::string name = "";
    std::string path = "";
    std::string buildId = "";
    std::string hash = "";

    static std::string GetNameFromPath(const std::string &path);
};

struct ElfLoadInfo {
    uint64_t offset = 0;
    uint64_t tableVaddr = 0;
    size_t tableSize = 0;
};

class DfxElfImpl {
public:
    DfxElfImpl(std::shared_ptr<DfxMemory> memory) : memory_(memory) {}
    virtual ~DfxElfImpl() = default;

    virtual bool Init();

    virtual bool IsValidPc(uint64_t pc);
    virtual void GetMaxSize(uint64_t* size);
    virtual uint64_t GetRealLoadOffset(uint64_t offset) const;

    virtual int64_t GetLoadBias();
    virtual bool GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* start, uint64_t* end);
    virtual bool GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* funcOffset);
    virtual bool GetGlobalVariableOffset(const std::string& name, uint64_t* offset);
    virtual std::string GetBuildID();

protected:
    bool ReadAllHeaders();
    bool ReadElfHeaders(ElfW(Ehdr)& ehdr);
    void ReadProgramHeaders(const ElfW(Ehdr)& ehdr);
    void ReadSectionHeaders(const ElfW(Ehdr)& ehdr);

private:
    int64_t loadBias_ = 0;
    uint64_t buildIdOffset_ = 0;
    uint64_t buildIdSize_ = 0;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    std::vector<SymbolInfo> symbols_;
    std::shared_ptr<DfxMemory> memory_;
};

class DfxElf {
public:
    DfxElf(std::shared_ptr<DfxMemory> memory) : memory_(memory) {}
    virtual ~DfxElf() = default;

    bool Init();
    bool IsValid() { return valid_; }

    bool IsValidPc(uint64_t pc);
    uint64_t GetRealLoadOffset(uint64_t offset) const;

    bool GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* start, uint64_t* end);
    bool GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* funcOffset);
    bool GetGlobalVariableOffset(const std::string& name, uint64_t* offset);
    std::string GetBuildID();
    int64_t GetLoadBias();
    uint16_t GetMachine() { return machine_; }
    uint8_t GetClass() { return class_; }
    ArchType GetArch() { return arch_; }

    static bool IsValidElf(std::shared_ptr<DfxMemory> memory);
    static uint64_t GetMaxSize(std::shared_ptr<DfxMemory> memory);
    static std::string GetReadableBuildID(const std::string &buildIdHex);
private:
    bool ReadElfInfo();
private:
    bool valid_ = false;
    uint16_t machine_ = 0;
    uint8_t class_ = 0;
    ArchType arch_;
    std::mutex lock_;
    std::unique_ptr<DfxElfImpl> elfImpl_;
    std::shared_ptr<DfxMemory> memory_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
