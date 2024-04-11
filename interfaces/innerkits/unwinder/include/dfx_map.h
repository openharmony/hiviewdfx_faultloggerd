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

#ifndef DFX_MAP_H
#define DFX_MAP_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>

namespace OHOS {
namespace HiviewDFX {
class DfxElf;
class DfxHap;

class DfxMap {
public:
    static std::shared_ptr<DfxMap> Create(const std::string buf, size_t size);
    static void PermsToProts(const std::string perms, uint32_t& prots, uint32_t& flag);

    DfxMap() = default;
    DfxMap(uint64_t begin, uint64_t end, uint64_t offset,
        const std::string& perms, const std::string& name)
        : begin(begin), end(end), offset(offset), perms(perms), name(name) {}
    DfxMap(uint64_t begin, uint64_t end, uint64_t offset,
        uint32_t prots, const std::string& name)
        : begin(begin), end(end), offset(offset), prots(prots), name(name) {}

    bool Parse(const std::string buf, size_t size);
    bool IsMapExec();
    bool IsArkExecutable();
    const std::shared_ptr<DfxHap> GetHap();
    const std::shared_ptr<DfxElf> GetElf(pid_t pid = 0);
    std::string GetElfName();
    uint64_t GetRelPc(uint64_t pc);
    std::string ToString();

    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    uint32_t prots = 0;
    uint32_t flag = 0;
    uint64_t major = 0;
    uint64_t minor = 0;
    ino_t inode = 0;
    std::string perms = ""; // 5:rwxp
    std::string name = "";
    std::shared_ptr<DfxElf> elf = nullptr;
    std::shared_ptr<DfxHap> hap = nullptr;
    std::shared_ptr<DfxMap> prevMap = nullptr;
    uint64_t elfOffset = 0;
    uint64_t elfStartOffset = 0;
    int32_t symbolFileIndex = -1; // symbols file index
#if is_ohos && !is_mingw
    std::shared_ptr<std::vector<uint8_t>> shmmData = nullptr;
#endif
    // use for find
    inline bool operator==(const std::string &name) const
    {
        return this->name == name;
    }

    inline bool operator<(const DfxMap &other) const
    {
        return this->end < other.end;
    }

    bool Contain(uint64_t pc) const
    {
        return (pc >= begin && pc < end);
    }

    // The range [first, last) must be partitioned with respect to the expression
    // !(value < element) or !comp(value, element)
    static bool ValueLessThen(uint64_t vaddr, const DfxMap &a)
    {
        return vaddr < a.begin;
    }
    static bool ValueLessEqual(uint64_t vaddr, const DfxMap &a)
    {
        return vaddr <= a.begin;
    }
    uint64_t FileOffsetFromAddr(uint64_t vaddr) const
    {
        // real vaddr - real map begin = addr offset in section
        // section offset + page off set = file offset
        return vaddr - begin + offset;
    }
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
