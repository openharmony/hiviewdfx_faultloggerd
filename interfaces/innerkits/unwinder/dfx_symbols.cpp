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

#include "dfx_symbols.h"

#include <algorithm>
#include <cstdlib>
#include <cxxabi.h>
#ifdef RUSTC_DEMANGLE
#include <dlfcn.h>
#endif

#include "dfx_define.h"
#include "dfx_log.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
using RustDemangleFn = char*(*)(const char *);
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxSymbols"

const std::string LINKER_PREFIX = "__dl_";
const std::string LINKER_PREFIX_NAME = "[linker]";

#ifdef RUSTC_DEMANGLE
static std::mutex g_mutex;
static bool g_hasTryLoadRustDemangleLib = false;
static RustDemangleFn g_rustDemangleFn = nullptr;
#endif
}

#ifdef RUSTC_DEMANGLE
bool DfxSymbols::FindRustDemangleFunction()
{
    if (g_hasTryLoadRustDemangleLib) {
        return (g_rustDemangleFn != nullptr);
    }

    g_hasTryLoadRustDemangleLib = true;
    void* rustDemangleLibHandle = dlopen("librustc_demangle.z.so", RTLD_LAZY | RTLD_NODELETE);
    if (rustDemangleLibHandle == nullptr) {
        LOGW("Failed to dlopen librustc_demangle, %s", dlerror());
        return false;
    }

    g_rustDemangleFn = (RustDemangleFn)dlsym(rustDemangleLibHandle, "rustc_demangle");
    if (g_rustDemangleFn == nullptr) {
        LOGW("Failed to dlsym rustc_demangle, %s", dlerror());
        dlclose(rustDemangleLibHandle);
        return false;
    }
    return true;
}
#endif

bool DfxSymbols::ParseSymbols(std::vector<DfxSymbol>& symbols, std::shared_ptr<DfxElf> elf, const std::string& filePath)
{
    if (elf == nullptr) {
        return false;
    }
    auto elfSymbols = elf->GetFuncSymbols();
    std::string symbolsPath = filePath;
    if (elf->GetBaseOffset() != 0) {
        symbolsPath += ("!" + elf->GetElfName());
    }
    for (auto elfSymbol : elfSymbols) {
        symbols.emplace_back(elfSymbol.value, elfSymbol.size,
            elfSymbol.nameStr, Demangle(elfSymbol.nameStr), symbolsPath);
    }
    return true;
}

bool DfxSymbols::AddSymbolsByPlt(std::vector<DfxSymbol>& symbols, std::shared_ptr<DfxElf> elf,
                                 const std::string& filePath)
{
    if (elf == nullptr) {
        return false;
    }
    ShdrInfo shdr;
    elf->GetSectionInfo(shdr, PLT);
    symbols.emplace_back(shdr.addr, shdr.size, PLT, filePath);
    return true;
}

bool DfxSymbols::GetFuncNameAndOffsetByPc(uint64_t relPc, std::shared_ptr<DfxElf> elf,
    std::string& funcName, uint64_t& funcOffset)
{
#if defined(__arm__)
    relPc = relPc | 1;
#endif
    ElfSymbol elfSymbol;
    if ((elf != nullptr) && elf->GetFuncInfo(relPc, elfSymbol)) {
        LOGU("nameStr: %s", elfSymbol.nameStr.c_str());
        funcName = Demangle(elfSymbol.nameStr);
        funcOffset = relPc - elfSymbol.value;
#if defined(__arm__)
        funcOffset &= ~1;
#endif
        LOGU("Symbol relPc: %" PRIx64 ", funcName: %s, funcOffset: %" PRIx64 "", relPc, funcName.c_str(), funcOffset);
        return true;
    }
    return false;
}

std::string DfxSymbols::Demangle(const std::string& buf)
{
    if ((buf.length() < 2) || (buf[0] != '_')) { // 2 : min buf length
        return buf;
    }

    std::string funcName;
    const char *bufStr = buf.c_str();
    if (StartsWith(buf, LINKER_PREFIX)) {
        bufStr += LINKER_PREFIX.size();
        funcName += LINKER_PREFIX_NAME;
    }

    int status = 0;
    char* demangledStr = nullptr;
    if (buf[1] == 'Z') {
        demangledStr = abi::__cxa_demangle(bufStr, nullptr, nullptr, &status);
    }
#ifdef RUSTC_DEMANGLE
    if (buf[1] == 'R') {
        std::lock_guard<std::mutex> lck(g_mutex);
        if (FindRustDemangleFunction()) {
            demangledStr = g_rustDemangleFn(bufStr);
        }
    }
#endif
    std::string demangleName;
    if (demangledStr != nullptr) {
        demangleName = std::string(demangledStr);
        std::free(demangledStr);
    } else {
        demangleName = std::string(bufStr);
    }
    funcName += demangleName;
    return funcName;
}
} // namespace HiviewDFX
} // namespace OHOS
