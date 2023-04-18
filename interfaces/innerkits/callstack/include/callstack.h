/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_HIVIEWDFX_CALLSTACK_H
#define OHOS_HIVIEWDFX_CALLSTACK_H

#if HAVE_LIBUNWIND
// for libunwind.h empty struct has size 0 in c, size 1 in c++
#define UNW_EMPTY_STRUCT uint8_t unused;
#include <libunwind.h>
#endif

#include <map>
#include <optional>
#include <string>
#include <vector>

#if !is_mingw
#include <sys/mman.h>
#endif

#include "hashlist.hpp"
#include "register.h"
#include "dfx_utilities.h"

namespace OHOS {
#ifdef HIPERF_USE_CALLSTACK
namespace Developtools {
namespace HiPerf {
class SymbolsFile;
class VirtualThread;
class MemMapItem;
struct CallFrame;
} // namespace HiPerf
} // namespace Developtools
#endif // HIPERF_USE_CALLSTACK

#ifdef NATIVEDAEMON_USE_CALLSTACK
namespace Developtools {
namespace NativeDaemon {
class SymbolsFile;
class VirtualThread;
class MemMapItem;
struct CallFrame;
} // namespace NativeDaemon
} // namespace Developtools
#endif // NATIVEDAEMON_USE_CALLSTACK

namespace HiviewDFX {
#ifdef HIPERF_USE_CALLSTACK
using namespace OHOS::Developtools::HiPerf;
const size_t MAX_CALL_FRAME_UNWIND_SIZE = 256;
#endif // HIPERF_USE_CALLSTACK

#ifdef NATIVEDAEMON_USE_CALLSTACK
using namespace OHOS::Developtools::NativeDaemon;
const size_t MAX_CALL_FRAME_UNWIND_SIZE = 102;
#endif // NATIVEDAEMON_USE_CALLSTACK

const int MAX_CALL_FRAME_EXPAND_CYCLE = 10;
const size_t MAX_CALL_FRAME_EXPAND_CACHE_SIZE = 10;

#if HAVE_LIBUNWIND
struct UnwindInfo;
#endif

class CallStack {
public:
    CallStack();
    ~CallStack();
    bool UnwindCallStack(const VirtualThread &thread, bool abi32, u64 *regs, u64 regsNum,
                         const u8 *stack, u64 stackSize, std::vector<CallFrame> &,
                         size_t maxStackLevel = MAX_CALL_FRAME_UNWIND_SIZE);
    bool UnwindCallStack(const VirtualThread &thread, u64 *regs, u64 regsNum,
                         const u8 *stack, u64 stackSize, std::vector<CallFrame> &,
                         size_t maxStackLevel = MAX_CALL_FRAME_UNWIND_SIZE);
    size_t ExpandCallStack(pid_t tid, std::vector<CallFrame> &callFrames, size_t expandLimit = 1u);

private:
#ifdef HIPERF_USE_CALLSTACK
    pid_t lastPid_ = -1;
    unw_word_t lastAddr_ = 0;
    unw_word_t lastData_ = 0;
#endif
    uint64_t stackPoint_ = 0;
    uint64_t stackEnd_ = 0;
    u64 *regs_ = nullptr; // not const , be cause we will fix it for arm64 cpu in UpdateRegForABI
    u64 regsNum_ = 0;
    const u8 *stack_ = nullptr;
    u64 stackSize_ = 0;

    void LogFrame(const std::string msg, const std::vector<CallFrame> &frames);
    size_t DoExpandCallStack(std::vector<CallFrame> &newCallFrames,
                             const std::vector<CallFrame> &cachedCallFrames, size_t expandLimit);

    // we have a cache for all thread
    std::map<pid_t, HashList<uint64_t, std::vector<CallFrame>>> cachedCallFramesMap_;
    bool GetIpSP(uint64_t &ip, uint64_t &sp, const u64 *regs, size_t regNum) const;
    ArchType arch_ = ArchType::UNSUPPORT;
#if HAVE_LIBUNWIND
    static bool ReadVirtualThreadMemory(UnwindInfo &unwindInfoPtr, unw_word_t addr,
                                        unw_word_t *data);
    static const std::string GetUnwErrorName(int error);
    static void dumpUDI(unw_dyn_info_t &di);
    static bool fillUDI(unw_dyn_info_t &di, SymbolsFile &symbolsFile, const MemMapItem &mmap,
                        const VirtualThread &thread);
    static int FindProcInfo(unw_addr_space_t as, unw_word_t ip, unw_proc_info_t *pi,
                            int need_unwind_info, void *arg);
    static int AccessMem(unw_addr_space_t as, unw_word_t addr, unw_word_t *valuePoint,
                         int writeOperation, void *arg);
    static int AccessReg(unw_addr_space_t as, unw_regnum_t regnum, unw_word_t *valuePoint,
                         int writeOperation, void *arg);
    static void PutUnwindInfo(unw_addr_space_t as, unw_proc_info_t *pi, void *arg);
    static int AccessFpreg(unw_addr_space_t as, unw_regnum_t num, unw_fpreg_t *val,
                           int writeOperation, void *arg);
    static int GetDynInfoListAaddr(unw_addr_space_t as, unw_word_t *dil_vaddr, void *arg);
    static int Resume(unw_addr_space_t as, unw_cursor_t *cu, void *arg);
    static int getProcName(unw_addr_space_t as, unw_word_t addr, char *bufp, size_t buf_len,
                           unw_word_t *offp, void *arg);
    static int FindUnwindTable(SymbolsFile *symbolsFile, const MemMapItem &mmap,
                               UnwindInfo *unwindInfoPtr, unw_addr_space_t as, unw_word_t ip,
                               unw_proc_info_t *pi, int need_unwind_info, void *arg);
    void UnwindStep(unw_cursor_t &c, std::vector<CallFrame> &callFrames, size_t maxStackLevel);
    std::unordered_map<pid_t, unw_addr_space_t> unwindAddrSpaceMap_;

    using dsoUnwDynInfoMap = std::unordered_map<std::string, std::optional<unw_dyn_info_t>>;
    std::unordered_map<pid_t, dsoUnwDynInfoMap> unwindDynInfoMap_;

    using unwMemoryCache = std::unordered_map<unw_word_t, unw_word_t>;
    std::unordered_map<pid_t, unwMemoryCache> porcessMemoryMap_;

    unw_accessors_t accessors_ = {
        .find_proc_info = FindProcInfo,
        .put_unwind_info = PutUnwindInfo,
        .get_dyn_info_list_addr = GetDynInfoListAaddr,
        .access_mem = AccessMem,
        .access_reg = AccessReg,
        .access_fpreg = AccessFpreg,
        .resume = Resume,
        .get_proc_name = getProcName,
    };
    bool DoUnwind(const VirtualThread &thread, std::vector<CallFrame> &callStack,
                  size_t maxStackLevel);
#endif
};

#if HAVE_LIBUNWIND
struct UnwindInfo {
    const VirtualThread &thread;
    const u64 *regs;
    size_t regNumber;
    ArchType arch;
    CallStack &callStack;
};
#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif // OHOS_HIVIEWDFX_CALLSTACK_H
