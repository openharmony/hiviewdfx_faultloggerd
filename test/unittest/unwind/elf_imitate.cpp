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

#include "elf_imitate.h"

#include <cstdlib>
#include <securec.h>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include <iostream>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

namespace {
const std::string EHDR_32 {"/data/test/resource/testdata/ehdr_from_readelf_32"};
const std::string EHDR_64 {"/data/test/resource/testdata/ehdr_from_readelf_64"};
const std::string SHDRS_32 {"/data/test/resource/testdata/shdrs_from_readelf_32"};
const std::string SHDRS_64 {"/data/test/resource/testdata/shdrs_from_readelf_64"};
const std::string PHDRS_32 {"/data/test/resource/testdata/phdrs_from_readelf_32"};
const std::string PHDRS_64 {"/data/test/resource/testdata/phdrs_from_readelf_64"};
const std::string SYMS_32 {"/data/test/resource/testdata/syms_from_readelf_32"};
const std::string SYMS_64 {"/data/test/resource/testdata/syms_from_readelf_64"};
} // namespace
namespace OHOS {
namespace HiviewDFX {
ElfImitate::~ElfImitate()
{
    if (ehdrFP_ != nullptr) {
        fclose(ehdrFP_);
        ehdrFP_ = nullptr;
    }
    if (shdrFP_ != nullptr) {
        fclose(shdrFP_);
        shdrFP_ = nullptr;
    }
    if (phdrFP_ != nullptr) {
        fclose(phdrFP_);
        phdrFP_ = nullptr;
    }
    if (symTabFP_ != nullptr) {
        fclose(symTabFP_);
        symTabFP_ = nullptr;
    }
}

static const std::string GetNextLine(FILE *fp, int *status)
{
    constexpr int bufSize {128};
    char buf[bufSize];
    if (memset_s(buf, sizeof(buf), '\0', sizeof(buf)) != EOK) {
        DFXLOG_ERROR("memset_s() failed");
        return "";
    }
    if (fgets(buf, bufSize, fp) == nullptr) {
        DFXLOG_ERROR("fgets() failed");
        *status = -1;
        return "";
    }
    *status = 0;
    std::string res {buf};
    if (res.back() == '\n') {
        res = res.substr(0, res.length() - 1);
    }
    return res;
}
std::vector<std::string> ElfImitate::StringSplit(std::string src, const std::string split)
{
    std::vector<std::string> result;

    if (!split.empty()) {
        size_t pos = 0;
        while ((pos = src.find(split)) != std::string::npos) {
            // split
            std::string token = src.substr(0, pos);
            if (!token.empty()) {
                result.push_back(token);
            }
            src.erase(0, pos + split.length());
        }
    }

    if (!src.empty()) {
        result.push_back(src);
    }
    return result;
}

bool ElfImitate::ParseAllHeaders(ElfFileType fileType)
{
    if (fileType == ElfFileType::ELF32) {
        ehdrFP_ = std::fopen(EHDR_32.c_str(), "rb");
        if (ehdrFP_ == nullptr) {
            DFXLOG_ERROR("fopen(EHDR_32, \"r\") failed");
        }
        shdrFP_ = fopen(SHDRS_32.c_str(), "rb");
        if (shdrFP_ == nullptr) {
            DFXLOG_ERROR("fopen(SHDRS_32, \"r\") failed");
        }
        phdrFP_ = fopen(PHDRS_32.c_str(), "rb");
        if (phdrFP_ == nullptr) {
            DFXLOG_ERROR("fopen(PHDRS_32, \"r\") failed");
        }
        symTabFP_ = fopen(SYMS_32.c_str(), "rb");
        if (symTabFP_ == nullptr) {
            DFXLOG_ERROR("fopen(SYMS_32, \"r\") failed");
        }
    } else if (fileType == ElfFileType::ELF64) {
        ehdrFP_ = fopen(EHDR_64.c_str(), "rb");
        if (ehdrFP_ == nullptr) {
            DFXLOG_ERROR("fopen(EHDR_64, \"r\") failed");
        }
        shdrFP_ = fopen(SHDRS_64.c_str(), "rb");
        if (shdrFP_ == nullptr) {
            DFXLOG_ERROR("fopen(SHDRS_64, \"r\") failed");
        }
        phdrFP_ = fopen(PHDRS_64.c_str(), "rb");
        if (phdrFP_ == nullptr) {
            DFXLOG_ERROR("fopen(PHDRS_64, \"r\") failed");
        }
        symTabFP_ = fopen(SYMS_64.c_str(), "rb");
        if (symTabFP_ == nullptr) {
            DFXLOG_ERROR("fopen(SYMS_64, \"r\") failed");
        }
    }
    if (!ParseElfHeaders()) {
        DFXLOG_WARN("ParseElfHeaders failed");
        return false;
    }

    if (!ParseProgramHeaders(fileType)) {
        DFXLOG_WARN("ParseProgramHeaders failed");
        return false;
    }

    if (!ParseSectionHeaders(fileType)) {
        DFXLOG_WARN("ReadSectionHeaders failed");
        return false;
    }
    if (!ParseElfSymbols()) {
        DFXLOG_WARN("ParseElfSymbols failed");
        return false;
    }
    return true;
}

bool ElfImitate::ParseElfHeaders()
{
    if (ehdrFP_ == nullptr) {
        DFXLOG_ERROR("param is null");
        return false;
    }
    int status {0};
    // drop header line
    GetNextLine(ehdrFP_, &status);
    if (!GetMagic(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitMagic(ehdrFP_) failed:");
        return false;
    }
    if (!GetClass(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitClass(ehdrFP_) failed:");
        return false;
    }
    constexpr int numSkip {6};
    // drop unused 6 lines
    for (int count = 0; count < numSkip; ++count) {
        GetNextLine(ehdrFP_, &status);
    }
    if (!GetMachine(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitMachine(ehdrFP_) failed:");
    }

    if (machine_ == "ARM") {
        archType_ = ARCH_ARM;
    } else if (machine_ == "80386") {
        archType_ = ARCH_X86;
    } else if (machine_ == "AARCH64") {
        archType_ = ARCH_ARM64;
    } else if (machine_ == "X86-64") {
        archType_ = ARCH_X86_64;
    } else {
        DFXLOG_WARN("Failed the machine = %d", machine_.c_str());
    }

    if (!GetEntryAddr(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitEntryAddr(ehdrFP_) failed:");
        return false;
    }
    if (!GetPrgOffset(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitPrgOffset(ehdrFP_) failed:");
        return false;
    }
    if (!GetSecOffset(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitSecOffset(ehdrFP_) failed:");
        return false;
    }
    if (!GetFlag(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitFlag(ehdrFP_) failed:");
        return false;
    }
    if (!GetEhdrSize(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitEhdrSize(ehdrFP_) failed:");
        return false;
    }
    if (!GetPhdrSize(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitPhdrSize(ehdrFP_) failed:");
        return false;
    }
    if (!GetNumPhdrs(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitNumPhdrs(ehdrFP_) failed:");
        return false;
    }
    if (!GetShdrSize(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitShdrSize(ehdrFP_) failed:");
        return false;
    }
    if (!GetNumShdrs(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitNumShdrs(ehdrFP_) failed:");
        return false;
    }
    if (!GetShdrStrTabIdx(ehdrFP_)) {
        DFXLOG_ERROR("ElfImitate::InitShdrStrTabIdx(ehdrFP_) failed:");
        return false;
    }
    elfSize_ = shdrOffset_ + shdrEntSize_ * shdrNumEnts_;
    return true;
}
bool ElfImitate::GetMagic(FILE * const fp)
{
    if (fp == nullptr) {
        DFXLOG_ERROR("param is null");
        return false;
    }
    int status {0};
    std::string magicLine = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto tmp = StringSplit(magicLine, " ");
    std::vector<std::string> strVec {tmp.begin() + 1, tmp.end()};
    if (strVec.size() != EI_NIDENT) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", magicLine.c_str());
        return false;
    }
    for (std::size_t count = 0; count < strVec.size(); ++count) {
        std::string valStr = strVec[count];
        constexpr int base {16};
        ehdrIdent_[count] = static_cast<unsigned char>(std::stoul(valStr, nullptr, base));
    }
    return true;
}

bool ElfImitate::GetClass(FILE * const fp)
{
    if (fp == nullptr) {
        DFXLOG_ERROR("param is null");
        return false;
    }
    int status {0};
    std::string classLine = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(classLine, " ");
    constexpr int len {2};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", classLine.c_str());
        return false;
    }
    if (strVec.back() == "ELF32") {
        classType_ = ELFCLASS32;
    } else if (strVec.back() == "ELF64") {
        classType_ = ELFCLASS64;
    }
    return true;
}

bool ElfImitate::GetMachine(FILE * const fp)
{
    int status {0};
    std::string machineLine = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(machineLine, " ");
    constexpr int len {2};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", machineLine.c_str());
        return false;
    }
    machine_ = strVec.back();
    return true;
}
bool ElfImitate::GetEntryAddr(FILE * const fp)
{
    int status {0};
    std::string entryLine = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(entryLine, " ");
    constexpr int len {2};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", entryLine.c_str());
        return false;
    }
    std::string entryStr = strVec.back();
    constexpr int base {16};
    prgEntryVaddr_ = static_cast<uint64_t>(std::stoull(entryStr, nullptr, base));
    return true;
}

bool ElfImitate::GetPrgOffset(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {5};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {1};
    std::string valStr = strVec[valIndex];
    phdrOffset_ = static_cast<uint64_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetSecOffset(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {8};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {4};
    std::string valStr = strVec[valIndex];
    shdrOffset_ = static_cast<uint64_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetFlag(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {2};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {1};
    std::string valStr = strVec[valIndex];
    ehdrFlags_ = static_cast<uint32_t>(std::stoul(valStr));
    return true;
}

bool ElfImitate::GetEhdrSize(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {6};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {4};
    std::string valStr = strVec[valIndex];
    ehdrSize_ = static_cast<uint16_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetPhdrSize(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {6};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {4};
    std::string valStr = strVec[valIndex];
    phdrEntSize_ = static_cast<uint16_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetNumPhdrs(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {5};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {4};
    std::string valStr = strVec[valIndex];
    phdrNumEnts_ = static_cast<uint16_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetShdrSize(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {6};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {4};
    std::string valStr = strVec[valIndex];
    shdrEntSize_ = static_cast<uint16_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetNumShdrs(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {5};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {4};
    std::string valStr = strVec[valIndex];
    shdrNumEnts_ = static_cast<uint16_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::GetShdrStrTabIdx(FILE * const fp)
{
    int status {0};
    std::string line = GetNextLine(fp, &status);
    if (status == -1) {
        DFXLOG_ERROR("early end");
        return false;
    }
    auto strVec = StringSplit(line, " ");
    constexpr int len {6};
    if (strVec.size() != len) {
        DFXLOG_ERROR("line format incorrect:");
        DFXLOG_ERROR("    line = %s", line.c_str());
        return false;
    }
    constexpr int valIndex {5};
    std::string valStr = strVec[valIndex];
    shdrStrTabIdx_ = static_cast<uint16_t>(std::stoull(valStr));
    return true;
}

bool ElfImitate::ParseProgramHeaders(ElfFileType fileType)
{
    std::string line {};
    while (true) {
        line = GetNextPhdrLine();
        if (line.empty()) {
            break;
            DFXLOG_INFO("no more program lines");
        }
        if (fileType == ElfFileType::ELF64) {
            int status = 0;
            std::string lineAppend = GetNextLine(phdrFP_, &status);
            if (status == -1) {
                DFXLOG_ERROR("GetNextLine(phdrFP_, &status) error:");
                line = "";
                break;
            }
            if (lineAppend.empty()) {
                break;
                DFXLOG_INFO("no more program lines");
            }
            line += lineAppend;
        }

        auto strVec = StringSplit(line, " ");
        std::string type = strVec[0];
        int base = 16;
        uint64_t offset =  std::stoull(strVec[1], nullptr, base);
        uint64_t vAddr = std::stoull(strVec[2], nullptr, base);
        uint64_t memSize = std::stoull(strVec[5], nullptr, base);
        std::string flg = strVec[6];
        if (!std::all_of(strVec[7].begin(), strVec[7].end(), ::isdigit)) {
            flg += strVec[7];
        }
        bool firstLoadHeader = true;
        if (type == "LOAD") {
            if (flg.find("E") == std::string::npos) {
                continue;
            }
            ptLoads_[offset] = ElfLoadInfo{offset, vAddr, static_cast<size_t>(memSize)};

            // Only set the load bias from the first executable load header.
            if (firstLoadHeader) {
                loadBias_ = static_cast<int64_t>(static_cast<uint64_t>(vAddr) - offset);
            }
            firstLoadHeader = false;

            if (vAddr < startVaddr_) {
                startVaddr_ = vAddr;
            }
            if (vAddr + memSize > endVaddr_) {
                endVaddr_ = vAddr + memSize;
            }
        }
    }
    return true;
}
bool ElfImitate::ParseSectionHeaders(ElfFileType fileType)
{
    std::string line {};
    int status = 0;
    (void)GetNextShdrLine(); //skip index 0 section header
    (void)GetNextLine(shdrFP_, &status);
    while (true) {
        int status {0};
        line = GetNextShdrLine();
        if (line.empty()) {
            break;
            DFXLOG_INFO("no more section lines");
        }
        if (fileType == ElfFileType::ELF64) {
            std::string lineAppend = GetNextLine(shdrFP_, &status);
            if (lineAppend.empty()) {
                break;
                DFXLOG_INFO("no more section lines");
            }
            line += lineAppend;
        }

        auto secIndex = GetSecIndex(line);

        auto pos = line.find("]");
        if (pos == std::string::npos) {
            DFXLOG_INFO("incorrect section line: %s", line.c_str());
            return false;
        }
        ++pos;
        std::string tmpLine = line.substr(pos, line.length() - pos);
        auto strVec = StringSplit(tmpLine, " ");
        for (size_t i = 0; i < strVec.size(); ++i) {}

        constexpr int base {16};
        std::string secName = strVec[0];
        std::string secType = strVec[1];
        uint64_t secAddr = std::stoull(strVec[2], nullptr, base);
        uint64_t secOffset = std::stoull(strVec[3], nullptr, base);
        uint64_t secSize = std::stoull(strVec[4], nullptr, base);
        uint64_t secEntSize = std::stoull(strVec[5], nullptr, base);
        uint64_t secLink = std::stoull(strVec[strVec.size() - 3], nullptr, base);
        uint64_t secInfo = std::stoull(strVec[strVec.size() - 2], nullptr, base);
        uint64_t secAddrAlign = std::stoull(strVec[strVec.size() - 1], nullptr, base);

        ShdrInfo shdrInfo;
        shdrInfo.addr = secAddr;
        shdrInfo.offset = secOffset;
        shdrInfo.size = secSize;
        shdrInfoPairs_.emplace(std::make_pair(secIndex, secName), shdrInfo);

        if (secType == "SYMTAB" || secType == "DYNSYM") {
            ElfShdr elfShdr;
            elfShdr.name = static_cast<uint32_t>(secIndex);
            if (secType == "SYMTAB") {
                elfShdr.type = static_cast<uint32_t>(SHT_SYMTAB);
            } else {
                elfShdr.type = static_cast<uint32_t>(SHT_DYNSYM);
            }

            elfShdr.addr = secAddr;
            elfShdr.offset = secOffset;
            elfShdr.size = secSize;
            elfShdr.link = static_cast<uint32_t>(secLink);
            elfShdr.info = static_cast<uint32_t>(secInfo);
            elfShdr.addrAlign = secAddrAlign;
            elfShdr.entSize = secEntSize;
            symShdrs_.emplace(secName, elfShdr);
        }
    }
    return true;
}
const std::string ElfImitate::GetNextPhdrLine()
{
    const std::string effectFlag {"0x00"};
    std::string line {};
    int status {0};
    while (true) {
        line = GetNextLine(phdrFP_, &status);
        if (status == -1) {
            DFXLOG_ERROR("GetNextLine(phdrFP_, &status) error:");
            line = "";
            break;
        }
        if (line.find(effectFlag) != std::string::npos) {
            DFXLOG_ERROR("effective program header line: %s", line.c_str());
            break;
        }
    }
    return line;
}

const std::string ElfImitate::GetNextShdrLine()
{
    const std::string effectFlag {"]"};
    std::string line {};
    int status {0};
    while (true) {
        line = GetNextLine(shdrFP_, &status);
        if (status == -1) {
            DFXLOG_ERROR("GetNextLine(phdrFP_, &status) error:");
            line = "";
            break;
        }
        auto pos = line.find(effectFlag);
        if ((pos != std::string::npos) and isdigit(line.at(pos - 1))) {
            DFXLOG_ERROR("effective section header line: %s", line.c_str());
            break;
        }
    }
    return line;
}
int64_t ElfImitate::GetSecIndex(const std::string &line)
{
    int64_t res {-1};
    auto pos = line.find("[");
    if (pos == std::string::npos) {
        DFXLOG_INFO("no section index found: %s", line.c_str());
        return res;
    }
    constexpr int len {4};
    std::string str = line.substr(pos, len);
    if (str.length() != len) {
        DFXLOG_INFO("section index form incorrect: %s", str.c_str());
        return res;
    }
    // section index is of the form "[xx]"
    constexpr int firstDigit {1};
    constexpr int numDigits {2};
    str = str.substr(firstDigit, numDigits);
    if (str[0] == ' ') {
        // str = [ x], transform it to [xx]
        str[0] = '0';
    }
    if (!str.empty() && std::all_of(str.begin(), str.end(), ::isdigit)) {
        res = std::stoll(str);
    } else {
        DFXLOG_INFO("not digits: %s", str.c_str());
    }
    return res;
}

bool ElfImitate::ParseElfSymbols()
{
    std::unordered_map <std::string, uint8_t> typeMap = {
        {"OBJECT", STT_OBJECT}, {"FUNC", STT_FUNC}, {"SECTION", STT_SECTION}, {"FILE", STT_FILE},
        {"COMMON", STT_COMMON}, {"TLS", STT_TLS}, {"NUM", STT_NUM}, {"LOOS", STT_LOOS},
        {"GNU_IFUNC", STT_GNU_IFUNC}, {"HIOS", STT_HIOS}, {"LOPROC", STT_LOPROC}, {"HIPROC", STT_HIPROC},
    };
    std::unordered_map <std::string, uint8_t> bindMap = {
        {"LOCAL", STB_LOCAL}, {"GLOBAL", STB_GLOBAL}, {"WEAK", STB_WEAK}, {"NUM", STB_NUM}, {"LOOS", STB_LOOS},
        {"GNU_UNIQUE", STB_GNU_UNIQUE}, {"HIOS", STB_HIOS}, {"LOPROC", STB_LOPROC}, {"HIPROC", STB_HIPROC}
    };
    std::unordered_map <std::string, uint8_t> vsMap = {
        {"DEFAULT", STV_DEFAULT}, {"INTERNAL", STV_INTERNAL}, {"HIDDEN", STV_HIDDEN}, {"PROTECTED", STV_PROTECTED},
    };
    std::string line {};
    while (true) {
        line = GetNextSymLine();
        if (line.empty()) {
            DFXLOG_INFO("no more symbol lines");
            break;
        }
        auto strVec = StringSplit(line, " ");
        ElfSymbol elfSymbol;
        constexpr int base {16};
        elfSymbol.name = std::stoul(strVec[0].substr(0, strVec[0].size() -1));
        elfSymbol.value = std::stoull(strVec[1], nullptr, base);
        elfSymbol.size = std::stoull(strVec[2]);
        elfSymbol.info = ELF32_ST_INFO(bindMap[strVec[4]], typeMap[strVec[3]]);
        elfSymbol.other = vsMap["strVec[5]"];
        if (strVec[6] == "UND") {
            elfSymbol.shndx = SHN_UNDEF;
        } else if (strVec[6] == "ABS") {
            elfSymbol.shndx = SHN_ABS;
        } else {
            elfSymbol.shndx = static_cast<uint16_t>(std::stoul(strVec[6]));
        }
        elfSymbols_.push_back(elfSymbol);
    }
    return true;
}
const std::string ElfImitate::GetNextSymLine()
{
    const std::string effectFlag {":"};
    std::string line {};
    int status {0};
    while (true) {
        line = GetNextLine(symTabFP_, &status);
        if (status == -1) {
            DFXLOG_INFO("GetNextLine(phdrFP_, &status) error:");
            line = "";
            break;
        }
        auto pos = line.find(effectFlag);
        if ((pos != std::string::npos) and isdigit(line.at(pos - 1))) {
            DFXLOG_INFO("effective symbol line: %s", line.c_str());
            break;
        }
    }
    return line;
}

bool ElfImitate::GetSectionInfo(ShdrInfo& shdr, const std::string secName)
{
    for (const auto &iter: shdrInfoPairs_)
    {
        auto tmpPair = iter.first;
        if (tmpPair.second == secName) {
            shdr = iter.second;
            return true;
        }
    }
    return false;
}

const std::vector<ElfSymbol>& ElfImitate::GetElfSymbols()
{
    if (elfSymbols_.empty()) {
        ParseElfSymbols();
    }
    return elfSymbols_;
}
uint64_t ElfImitate::GetLoadBase(uint64_t mapStart, uint64_t mapOffset)
{
    loadBase_ = mapStart - mapOffset - GetLoadBias();

    return loadBase_;
}

uint64_t ElfImitate::GetStartPc()
{
    auto startVaddr = GetStartVaddr();
    startPc_ = startVaddr + loadBase_;
    return startPc_;
}

uint64_t ElfImitate::GetEndPc()
{
    auto endVaddr = GetEndVaddr();
    endPc_ = endVaddr + loadBase_;
    return endPc_;
}

uint64_t ElfImitate::GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset)
{
    return (pc - GetLoadBase(mapStart, mapOffset));
}

bool ElfImitate::IsFunc(const ElfSymbol symbol)
{
    return ((symbol.shndx != SHN_UNDEF) &&
        (ELF32_ST_TYPE(symbol.info) == STT_FUNC || ELF32_ST_TYPE(symbol.info) == STT_GNU_IFUNC));
}

bool ElfImitate::ParseSymbols(std::vector<DfxSymbol>& symbols, const std::string& filePath)
{
    std::vector<ElfSymbol> elfSymbols = GetElfSymbols();
    for (auto elfSymbol : elfSymbols) {
        if (IsFunc(elfSymbol)) {
            if (elfSymbol.value == 0) {
                continue;
            }
            std::string nameStr = "";
            symbols.emplace_back(elfSymbol.value, elfSymbol.size,
                                 nameStr, DfxSymbols::Demangle(nameStr), filePath);
        } else {
            continue;
        }
    }
    auto comp = [](DfxSymbol a, DfxSymbol b) { return a.funcVaddr_ < b.funcVaddr_; };
    std::sort(symbols.begin(), symbols.end(), comp);
    auto pred = [](DfxSymbol a, DfxSymbol b) { return a.funcVaddr_ == b.funcVaddr_; };
    symbols.erase(std::unique(symbols.begin(), symbols.end(), pred), symbols.end());
    symbols.shrink_to_fit();
    return true;
}

bool ElfImitate::AddSymbolsByPlt(std::vector<DfxSymbol>& symbols, const std::string& filePath)
{
    ShdrInfo shdr;
    GetSectionInfo(shdr, PLT);
    symbols.emplace_back(shdr.addr, shdr.size, PLT, filePath);
    return true;
}

bool ElfImitate::GetFuncNameAndOffset(uint64_t pc, std::string& funcName, uint64_t& start, uint64_t& end)
{
    std::vector<DfxSymbol> symbols;
    if (!ParseSymbols(symbols, "")) {
        return false;
    }

    // TODO: use binary search ?
    for (const auto& symbol : symbols) {
        if (symbol.Contain(pc)) {
            funcName = symbol.demangle_;
            start = symbol.funcVaddr_;
            end = symbol.funcVaddr_ + symbol.size_;
            return true;
        }
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS