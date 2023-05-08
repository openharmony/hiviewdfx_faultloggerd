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

#include "callstack.h"

#include "callstack_log.h"
#include "mem_map_item.h"
#include "perf_event_record.h"
#include "symbols_file.h"
#include "virtual_thread.h"

#include <string>
#if HAVE_LIBUNWIND
#include <libunwind.h>
extern "C" {
#include <libunwind_i.h>
}
#endif

#include "register.h"
#ifdef target_cpu_arm
// reg size is int (unw_word_t)
#define UNW_WORD_PFLAG "x"
#else
// reg size is long (unw_word_t)
#define UNW_WORD_PFLAG "zx"
#endif
namespace OHOS {
namespace HiviewDFX {
const std::string ARM_EXIDX = ".ARM.exidx";

#if HAVE_LIBUNWIND
const std::map<unw_error_t, const std::string> UNW_ERROR_MAP = {
    {UNW_ESUCCESS, std::to_string(UNW_ESUCCESS)},
    {UNW_EUNSPEC, std::to_string(UNW_EUNSPEC)},
    {UNW_ENOMEM, std::to_string(UNW_ENOMEM)},
    {UNW_EBADREG, std::to_string(UNW_EBADREG)},
    {UNW_EREADONLYREG, std::to_string(UNW_EREADONLYREG)},
    {UNW_ESTOPUNWIND, std::to_string(UNW_ESTOPUNWIND)},
    {UNW_EINVALIDIP, std::to_string(UNW_EINVALIDIP)},
    {UNW_EBADFRAME, std::to_string(UNW_EBADFRAME)},
    {UNW_EINVAL, std::to_string(UNW_EINVAL)},
    {UNW_EBADVERSION, std::to_string(UNW_EBADVERSION)},
    {UNW_ENOINFO, std::to_string(UNW_ENOINFO)},
};
const std::string CallStack::GetUnwErrorName(int error)
{
    if (UNW_ERROR_MAP.count(static_cast<unw_error_t>(-error)) > 0) {
        return UNW_ERROR_MAP.at(static_cast<unw_error_t>(-error));
    } else {
        return "UNKNOW_UNW_ERROR";
    }
}

void CallStack::dumpUDI(unw_dyn_info_t &di)
{
    DFXLOG_DEBUG("unwind_table info: ");
    DFXLOG_DEBUG(" di.start_ip:            0x%016" UNW_WORD_PFLAG "", di.start_ip);
    DFXLOG_DEBUG(" di.end_ip:              0x%016" UNW_WORD_PFLAG "", di.end_ip);
    DFXLOG_DEBUG(" di.u.rti.segbase:       0x%016" UNW_WORD_PFLAG "", di.u.rti.segbase);
    DFXLOG_DEBUG(" di.u.rti.table_data:    0x%016" UNW_WORD_PFLAG "", di.u.rti.table_data);
    DFXLOG_DEBUG(" di.u.rti.table_len:     0x%016" UNW_WORD_PFLAG "", di.u.rti.table_len);
}

#ifdef NATIVEDAEMON_USE_CALLSTACK
bool CallStack::fillUDI(unw_dyn_info_t &di, SymbolsFile &symbolsFile, std::pair<MemMaps*, uint32_t> curMemMapsPair,
                        const VirtualThread &thread)
{
    uint64_t fdeTableElfOffset = 0;
    uint64_t fdeTableSize = 0;
    uint64_t ehFrameHdrElfOffset = 0;
    uint64_t SectionVaddr = 0;
    uint64_t SectionSize = 0;
    uint64_t SectionFileOffset = 0;
    auto [curMemMaps, itemIndex] = curMemMapsPair;
    di.start_ip = curMemMaps->maps_[itemIndex].begin_;
    di.end_ip = curMemMaps->maps_[itemIndex].end_;
#ifndef __arm__
    if ((UNW_INFO_FORMAT_REMOTE_TABLE == di.format) &&
        symbolsFile.GetHDRSectionInfo(ehFrameHdrElfOffset, fdeTableElfOffset, fdeTableSize)) {
        /*
            unw_word_t name_ptr;        // addr. of table name (e.g., library name)
            unw_word_t segbase;         // segment base
            unw_word_t table_len;       // must be a multiple of sizeof(unw_word_t)!
            unw_word_t table_data;
        */
        /*
            all the rti addr is offset of the elf file
            begin - page offset = elf file base addr in vaddr user space
            begin - page offset + elf offset = vaddr in real word.(for this thread)
        */

        // segbase is file offset .
        /*
            00200000-00344000 r--p 00000000 08:02 46404365
            00344000-005c4000 r-xp 00143000 08:02 46404365

            LOAD           0x00000000001439c0 0x00000000003449c0 0x00000000003449c0
                            0x000000000027f3c0 0x000000000027f3c0  R E    0x1000

            GNU_EH_FRAME   0x00000000000f3248 0x00000000002f3248 0x00000000002f3248
                            0x000000000000bb04 0x000000000000bb04  R      0x4

        */
        const int32_t ehFrameMmapIndex = thread.FindMapByOffset(curMemMaps, ehFrameHdrElfOffset);

        if (ehFrameMmapIndex == -1) {
            DFXLOG_ERROR("no ehframe mmap found.");
            return false;
        }

        const auto& ehFrameMmap = curMemMaps->maps_[ehFrameMmapIndex];

        di.u.rti.segbase = ehFrameMmap.begin_ + ehFrameHdrElfOffset - ehFrameMmap.pageoffset_;
        di.u.rti.table_data = ehFrameMmap.begin_ + fdeTableElfOffset - ehFrameMmap.pageoffset_;
        di.u.rti.table_len = fdeTableSize / sizeof(unw_word_t);

        DFXLOG_DEBUG(" map pageoffset:         0x%016" PRIx64 "", curMemMaps->maps_[itemIndex].pageoffset_);
        DFXLOG_DEBUG(" ehFrameHdrElfOffset:    0x%016" PRIx64 "", ehFrameHdrElfOffset);
        DFXLOG_DEBUG(" fdeTableElfOffset:      0x%016" PRIx64 "", fdeTableElfOffset);
        DFXLOG_DEBUG(" fdeTableSize:           0x%016" PRIx64 "", fdeTableSize);
        return true;
    } else {
        DFXLOG_DEBUG("SymbolsFile::GetHDRSectionInfo() failed");
    }
#else
    if ((UNW_INFO_FORMAT_ARM_EXIDX == di.format) &&
        symbolsFile.GetSectionInfo(ARM_EXIDX, SectionVaddr, SectionSize, SectionFileOffset)) {
        const int32_t targetIndex = thread.FindMapByOffset(curMemMaps, SectionFileOffset);
        if (targetIndex == -1) {
            DFXLOG_ERROR("no debug mmap found.");
            return false;
        }
        const auto& targetMmap = curMemMaps->maps_[targetIndex];
        DFXLOG_DEBUG(" begin: %" PRIx64 " offset:%" PRIx64 "", targetMmap.begin_, targetMmap.pageoffset_);

        di.u.rti.table_data = targetMmap.begin_ + SectionFileOffset - targetMmap.pageoffset_;
        di.u.rti.table_len = SectionSize;
        DFXLOG_DEBUG(" SectionName:           %s", std::string(ARM_EXIDX).c_str());
        DFXLOG_DEBUG(" SectionVaddrt:         0x%016" PRIx64 "", SectionVaddr);
        DFXLOG_DEBUG(" SectionFileOffset      0x%016" PRIx64 "", SectionFileOffset);
        DFXLOG_DEBUG(" SectionSize:           0x%016" PRIx64 "", SectionSize);

        // GetSectionInfo return true, but SectionVaddr || SectionSize is 0 ???
        LOG_ASSERT(SectionVaddr != 0 && SectionSize != 0);
        return true;
    } else {
        DFXLOG_DEBUG("SymbolsFile::GetSectionInfo() failed");
    }
#endif
    return false;
}
#else
bool CallStack::fillUDI(unw_dyn_info_t &di, SymbolsFile &symbolsFile, const MemMapItem &mmap,
                        const VirtualThread &thread)
{
    uint64_t fdeTableElfOffset = 0;
    uint64_t fdeTableSize = 0;
    uint64_t ehFrameHdrElfOffset = 0;
    uint64_t SectionVaddr = 0;
    uint64_t SectionSize = 0;
    uint64_t SectionFileOffset = 0;
    di.start_ip = mmap.begin_;
    di.end_ip = mmap.end_;
#ifndef __arm__
    if ((UNW_INFO_FORMAT_REMOTE_TABLE == di.format) &&
        symbolsFile.GetHDRSectionInfo(ehFrameHdrElfOffset, fdeTableElfOffset, fdeTableSize)) {
        /*
            unw_word_t name_ptr;        // addr. of table name (e.g., library name)
            unw_word_t segbase;         // segment base
            unw_word_t table_len;       // must be a multiple of sizeof(unw_word_t)!
            unw_word_t table_data;
        */
        /*
            all the rti addr is offset of the elf file
            begin - page offset = elf file base addr in vaddr user space
            begin - page offset + elf offset = vaddr in real word.(for this thread)
        */

        // segbase is file offset .
        /*
            00200000-00344000 r--p 00000000 08:02 46404365
            00344000-005c4000 r-xp 00143000 08:02 46404365

            LOAD           0x00000000001439c0 0x00000000003449c0 0x00000000003449c0
                            0x000000000027f3c0 0x000000000027f3c0  R E    0x1000

            GNU_EH_FRAME   0x00000000000f3248 0x00000000002f3248 0x00000000002f3248
                            0x000000000000bb04 0x000000000000bb04  R      0x4

        */
        const MemMapItem *ehFrameMmap = thread.FindMapByFileInfo(mmap.name_, ehFrameHdrElfOffset);

        if (ehFrameMmap == nullptr) {
            DFXLOG_ERROR("no ehframe mmap found.");
            return false;
        }

        di.u.rti.segbase = ehFrameMmap->begin_ + ehFrameHdrElfOffset - ehFrameMmap->pageoffset_;
        di.u.rti.table_data = ehFrameMmap->begin_ + fdeTableElfOffset - ehFrameMmap->pageoffset_;
        di.u.rti.table_len = fdeTableSize / sizeof(unw_word_t);

        DFXLOG_DEBUG(" map pageoffset:         0x%016" PRIx64 "", mmap.pageoffset_);
        DFXLOG_DEBUG(" ehFrameHdrElfOffset:    0x%016" PRIx64 "", ehFrameHdrElfOffset);
        DFXLOG_DEBUG(" fdeTableElfOffset:      0x%016" PRIx64 "", fdeTableElfOffset);
        DFXLOG_DEBUG(" fdeTableSize:           0x%016" PRIx64 "", fdeTableSize);
        return true;
    } else {
        DFXLOG_DEBUG("SymbolsFile::GetHDRSectionInfo() failed");
    }
#else
    if ((UNW_INFO_FORMAT_ARM_EXIDX == di.format) &&
        symbolsFile.GetSectionInfo(ARM_EXIDX, SectionVaddr, SectionSize, SectionFileOffset)) {
        const MemMapItem *targetMmap = thread.FindMapByFileInfo(mmap.name_, SectionFileOffset);
        if (targetMmap == nullptr) {
            DFXLOG_ERROR("no debug mmap found.");
            return false;
        }
        DFXLOG_DEBUG(" begin: %" PRIx64 " offset:%" PRIx64 "", targetMmap->begin_, targetMmap->pageoffset_);

        di.u.rti.table_data = targetMmap->begin_ + SectionFileOffset - targetMmap->pageoffset_;
        di.u.rti.table_len = SectionSize;
        DFXLOG_DEBUG(" SectionName:           %s", std::string(ARM_EXIDX).c_str());
        DFXLOG_DEBUG(" SectionVaddrt:         0x%016" PRIx64 "", SectionVaddr);
        DFXLOG_DEBUG(" SectionFileOffset      0x%016" PRIx64 "", SectionFileOffset);
        DFXLOG_DEBUG(" SectionSize:           0x%016" PRIx64 "", SectionSize);

        // GetSectionInfo return true, but SectionVaddr || SectionSize is 0 ???
        LOG_ASSERT(SectionVaddr != 0 && SectionSize != 0);
        return true;
    } else {
        DFXLOG_DEBUG("SymbolsFile::GetSectionInfo() failed");
    }
#endif
    return false;
}
#endif // NATIVEDAEMON_USE_CALLSTACK

/*
    https://www.nongnu.org/libunwind/man/libunwind-dynamic(3).html
*/
#ifdef NATIVEDAEMON_USE_CALLSTACK
int CallStack::FindUnwindTable(SymbolsFile *symbolsFile, std::pair<MemMaps*, uint32_t> curMemMapsPair,
                               UnwindInfo *unwindInfoPtr, unw_addr_space_t as, unw_word_t ip,
                               unw_proc_info_t *pi, int need_unwind_info, void *arg)
#else
int CallStack::FindUnwindTable(SymbolsFile *symbolsFile, const MemMapItem &mmap,
                               UnwindInfo *unwindInfoPtr, unw_addr_space_t as, unw_word_t ip,
                               unw_proc_info_t *pi, int need_unwind_info, void *arg)
#endif // NATIVEDAEMON_USE_CALLSTACK
{
    DFXLOG_DEBUG("try search debug info at %s", symbolsFile->filePath_.c_str());
    auto &dynInfoProcessMap = unwindInfoPtr->callStack.unwindDynInfoMap_;
    // all the thread in same process have same mmap and symbols
    if (dynInfoProcessMap.find(unwindInfoPtr->thread.pid_) == dynInfoProcessMap.end()) {
        dynInfoProcessMap.emplace(unwindInfoPtr->thread.pid_, dsoUnwDynInfoMap {});
    }
    dsoUnwDynInfoMap &dynFileMap = dynInfoProcessMap[unwindInfoPtr->thread.pid_];
    // find use dso name as key
    if (dynFileMap.find(symbolsFile->filePath_) == dynFileMap.end()) {
        // we make a option empty value first
        std::optional<unw_dyn_info_t> &odi = dynFileMap[symbolsFile->filePath_];

        unw_dyn_info_t newdi;
        if (memset_s(&newdi, sizeof(unw_dyn_info_t), 0, sizeof(unw_dyn_info_t)) != EOK) {
            DFXLOG_ERROR("memset_s failed");
            return -UNW_EUNSPEC;
        }
#ifdef __arm__
        // arm use .ARM.exidx , not use ehframe
        newdi.format = UNW_INFO_FORMAT_ARM_EXIDX;
#else
        // otherwise we use EH FRAME
        newdi.format = UNW_INFO_FORMAT_REMOTE_TABLE;
#endif
#ifdef NATIVEDAEMON_USE_CALLSTACK
        if (fillUDI(newdi, *symbolsFile, curMemMapsPair, unwindInfoPtr->thread)) {
#else
        if (fillUDI(newdi, *symbolsFile, mmap, unwindInfoPtr->thread)) {
#endif // NATIVEDAEMON_USE_CALLSTACK
            dumpUDI(newdi);
            odi = newdi;
        } else {
            DFXLOG_ERROR("fillUDI failed()");
            return -UNW_EUNSPEC;
        }
    }

    LOG_ASSERT(dynInfoProcessMap.find(unwindInfoPtr->thread.pid_) != dynInfoProcessMap.end());
    LOG_ASSERT_MESSAGE(dynFileMap.find(symbolsFile->filePath_) != dynFileMap.end(), "%s",
        symbolsFile->filePath_.c_str());
    std::optional<unw_dyn_info_t> &odi =
        dynInfoProcessMap.at(unwindInfoPtr->thread.pid_).at(symbolsFile->filePath_);

    if (odi.has_value()) {
        unw_dyn_info_t &di = odi.value();
        /*
            we don't use dwarf_search_unwind_table
            because in arm it will search two function:
            1 arm_search_unwind_table first
            2 dwarf_search_unwind_table

            see libunwind_i.h for arm
            define tdep_search_unwind_table UNW_OBJ(search_unwind_table)

        */
        int ret = static_cast<unw_error_t>(
            tdep_search_unwind_table(as, ip, &di, pi, need_unwind_info, arg));

        DFXLOG_DEBUG("search_unwind_table ret %d:%s", ret, GetUnwErrorName(ret).c_str());

        if (UNW_ESUCCESS != ret) {
            if (UNW_ENOINFO != ret) {
                DFXLOG_WARN("search_unwind_table ret error %d:%s", ret, GetUnwErrorName(ret).c_str());
            }
            return -UNW_EUNSPEC;
        } else {
            return UNW_ESUCCESS;
        }
    } else {
        DFXLOG_WARN("no debug info found for thread %d:%s", unwindInfoPtr->thread.tid_,
            unwindInfoPtr->thread.name_.c_str());
        return -UNW_EUNSPEC;
    }
}

int CallStack::FindProcInfo(unw_addr_space_t as, unw_word_t ip, unw_proc_info_t *pi,
                            int need_unwind_info, void *arg)
{
    UnwindInfo *unwindInfoPtr = static_cast<UnwindInfo *>(arg);

    DFXLOG_DEBUG("need_unwind_info ret %d ip %" UNW_WORD_PFLAG "", need_unwind_info, ip);
#ifdef NATIVEDAEMON_USE_CALLSTACK
    auto curMemMapsPair = unwindInfoPtr->thread.FindMemMapsByAddr(ip);
    auto [curMemMaps, itemIndex] = curMemMapsPair;
    if (curMemMaps != nullptr) {
        SymbolsFile *symbolsFile = unwindInfoPtr->thread.FindSymbolsFileByName(curMemMaps->name_);
        if (symbolsFile != nullptr) {
            return FindUnwindTable(symbolsFile, curMemMapsPair, unwindInfoPtr, as, ip, pi,
                                    need_unwind_info, arg);
#else
    const MemMapItem *mmap = unwindInfoPtr->thread.FindMapByAddr(ip);
    if (mmap != nullptr) {
        SymbolsFile *symbolsFile = unwindInfoPtr->thread.FindSymbolsFileByMap(*mmap);
        if (symbolsFile != nullptr) {
            return FindUnwindTable(symbolsFile, *mmap, unwindInfoPtr, as, ip, pi, need_unwind_info, arg);
#endif // NATIVEDAEMON_USE_CALLSTACK
        } else {
            DFXLOG_WARN("no symbols file found for thread %d:%s", unwindInfoPtr->thread.tid_,
                unwindInfoPtr->thread.name_.c_str());
        }
    } else {
        DFXLOG_ERROR("ip 0x%016" UNW_WORD_PFLAG " not found in thread %d:%s", ip,
            unwindInfoPtr->thread.tid_, unwindInfoPtr->thread.name_.c_str());
    }

    return -UNW_EUNSPEC;
}

bool CallStack::ReadVirtualThreadMemory(UnwindInfo &unwindInfoPtr, unw_word_t vaddr,
                                        unw_word_t *data)
{
#if defined(HIPERF_USE_CALLSTACK)
    if (__builtin_expect(unwindInfoPtr.thread.pid_ == unwindInfoPtr.callStack.lastPid_ &&
        vaddr == unwindInfoPtr.callStack.lastAddr_, true)) {
        *data = unwindInfoPtr.callStack.lastData_;
        return true;
    }

    if (unwindInfoPtr.thread.ReadRoMemory(vaddr, reinterpret_cast<uint8_t*>(data), sizeof(unw_word_t))) {
        unwindInfoPtr.callStack.lastPid_ = unwindInfoPtr.thread.pid_;
        unwindInfoPtr.callStack.lastAddr_ = vaddr;
        unwindInfoPtr.callStack.lastData_ = *data;
        return true;
    } else {
        unwindInfoPtr.callStack.lastPid_ = -1;
        unwindInfoPtr.callStack.lastAddr_ = 0;
        return false;
    }
#elif defined(NATIVEDAEMON_USE_CALLSTACK)
    auto process = unwindInfoPtr.callStack.porcessMemoryMap_.find(unwindInfoPtr.thread.pid_);
    if (process != unwindInfoPtr.callStack.porcessMemoryMap_.end()) {
        auto memory = process->second.find(vaddr);
        if (memory != process->second.end()) {
            *data = memory->second;
            return true;
        }
    }

    if (unwindInfoPtr.thread.ReadRoMemory(vaddr, (uint8_t *)data, sizeof(unw_word_t))) {
        unwindInfoPtr.callStack.porcessMemoryMap_[unwindInfoPtr.thread.pid_][vaddr] = *data;
        return true;
    } else {
        return false;
    }
#else
#error
#endif
}

int CallStack::AccessMem([[maybe_unused]] unw_addr_space_t as, unw_word_t addr,
                         unw_word_t *valuePoint, int writeOperation, void *arg)
{
    UnwindInfo *unwindInfoPtr = static_cast<UnwindInfo *>(arg);
    size_t stackOffset = 0;
    *valuePoint = 0;
#ifdef NATIVEDAEMON_USE_CALLSTACK
    DFXLOG_DEBUG("try access addr 0x%" UNW_WORD_PFLAG " ", addr);
    LOG_ASSERT(writeOperation == 0);
#endif

    /* Check overflow. */
    if (addr + sizeof(unw_word_t) < addr) {
        DFXLOG_ERROR("address overfolw at 0x%" UNW_WORD_PFLAG " increase 0x%zu", addr, sizeof(unw_word_t));
        return -UNW_EUNSPEC;
    }

    if (addr < unwindInfoPtr->callStack.stackPoint_ or
        addr + sizeof(unw_word_t) >= unwindInfoPtr->callStack.stackEnd_) {
        if (ReadVirtualThreadMemory(*unwindInfoPtr, addr, valuePoint)) {
            DFXLOG_DEBUG("access_mem addr %p get val 0x%" UNW_WORD_PFLAG ", from mmap",
                reinterpret_cast<void *>(addr), *valuePoint);
        } else {
            DFXLOG_WARN("access_mem addr %p failed, from mmap, STACK RANGE 0x%" PRIx64 "- 0x%" PRIx64 "(0x%" PRIx64 ")",
                  reinterpret_cast<void *>(addr),
                  unwindInfoPtr->callStack.stackPoint_, unwindInfoPtr->callStack.stackEnd_,
                  unwindInfoPtr->callStack.stackEnd_ - unwindInfoPtr->callStack.stackPoint_);
            return -UNW_EUNSPEC;
        }
    } else {
        stackOffset = addr - unwindInfoPtr->callStack.stackPoint_;
        *valuePoint = *(unw_word_t *)&unwindInfoPtr->callStack.stack_[stackOffset];
        DFXLOG_DEBUG("access_mem addr %p val %" UNW_WORD_PFLAG ", from stack offset %zu",
            reinterpret_cast<void *>(addr), *valuePoint, stackOffset);
    }

    return UNW_ESUCCESS;
}

int CallStack::AccessReg([[maybe_unused]] unw_addr_space_t as, unw_regnum_t regnum,
                         unw_word_t *valuePoint, int writeOperation, void *arg)
{
    UnwindInfo *unwindInfoPtr = static_cast<UnwindInfo *>(arg);
    uint64_t val;
    size_t perfRegIndex = LibunwindRegIdToPerfReg(regnum);
#if defined(HIPERF_USE_CALLSTACK)
    if (perfRegIndex < 0) {
        DFXLOG_ERROR("can't read reg %d", perfRegIndex);
        return perfRegIndex;
    }
#elif defined(NATIVEDAEMON_USE_CALLSTACK)
    if (perfRegIndex < PERF_REG_ARM64_X29) {
        // libunwind not access other regs
        DFXLOG_ERROR("access_reg not expected %d", regnum);
    }
#else
#error
#endif

    /* Don't support write, I suspect we don't need it. */
    if (writeOperation) {
        DFXLOG_ERROR("access_reg %d", regnum);
        return -UNW_EINVAL;
    }

    if (unwindInfoPtr->callStack.regsNum_ == 0) {
        return -UNW_EUNSPEC;
    }

    if (!RegisterGetValue(val, unwindInfoPtr->callStack.regs_, perfRegIndex, unwindInfoPtr->callStack.regsNum_)) {
        DFXLOG_ERROR("can't read reg %zu", perfRegIndex);
        return -UNW_EUNSPEC;
    }

    *valuePoint = (unw_word_t)val;
    DFXLOG_DEBUG("reg %d:%s, val 0x%" UNW_WORD_PFLAG "", regnum,
        RegisterGetName(static_cast<size_t>(perfRegIndex)).c_str(), *valuePoint);
    return UNW_ESUCCESS;
}

void CallStack::PutUnwindInfo([[maybe_unused]] unw_addr_space_t as,
                              [[maybe_unused]] unw_proc_info_t *pi, [[maybe_unused]] void *arg)
{
}

int CallStack::AccessFpreg([[maybe_unused]] unw_addr_space_t as, [[maybe_unused]] unw_regnum_t num,
                           [[maybe_unused]] unw_fpreg_t *val, [[maybe_unused]] int writeOperation,
                           [[maybe_unused]] void *arg)
{
    return -UNW_EINVAL;
}

int CallStack::GetDynInfoListAaddr([[maybe_unused]] unw_addr_space_t as,
                                   [[maybe_unused]] unw_word_t *dil_vaddr,
                                   [[maybe_unused]] void *arg)
{
    return -UNW_ENOINFO;
}

int CallStack::Resume([[maybe_unused]] unw_addr_space_t as, [[maybe_unused]] unw_cursor_t *cu,
                      [[maybe_unused]] void *arg)
{
    return -UNW_EINVAL;
}

int CallStack::getProcName([[maybe_unused]] unw_addr_space_t as, [[maybe_unused]] unw_word_t addr,
                           [[maybe_unused]] char *bufp, [[maybe_unused]] size_t buf_len,
                           [[maybe_unused]] unw_word_t *offp, [[maybe_unused]] void *arg)
{
    return -UNW_EINVAL;
}

void CallStack::UnwindStep(unw_cursor_t &c, std::vector<CallFrame> &callStack, size_t maxStackLevel)
{
    while (callStack.size() < maxStackLevel) {
        int ret = unw_step(&c);
        if (ret > 0) {
            unw_word_t ip = 0;
            unw_word_t sp = 0;
            unw_get_reg(&c, UNW_REG_IP, &ip);
            unw_get_reg(&c, UNW_REG_SP, &sp);

            if (ip == 0) {
                DFXLOG_DEBUG("ip == 0 something is wrong. break");
                break;
            }

            /*
             * Decrement the IP for any non-activation frames.
             * this is required to properly find the srcline
             * for caller frames.
             * See also the documentation for dwfl_frame_pc(),
             * which this code tries to replicate.
             */
            if (unw_is_signal_frame(&c) <= 0) {
                --ip;
            }
            DFXLOG_DEBUG("unwind:%zu: ip 0x%" UNW_WORD_PFLAG " sp 0x%" UNW_WORD_PFLAG "", callStack.size(), ip, sp);
            if (callStack.back().ip_ == ip && callStack.back().sp_ == sp) {
                DFXLOG_WARN("we found a same frame, stop here");
                break;
            }
            callStack.emplace_back(ip, sp);
        } else {
            DFXLOG_DEBUG("no more frame step found. ret %d:%s", ret, GetUnwErrorName(ret).c_str());
            break;
        }
    }
}
#endif

bool CallStack::GetIpSP(uint64_t &ip, uint64_t &sp, const u64 *regs, size_t regNum) const
{
    if (regNum > 0) {
        if (!RegisterGetSPValue(sp, arch_, regs, regNum)) {
            DFXLOG_WARN("unable get sp");
            return false;
        }
        if (!RegisterGetIPValue(ip, arch_, regs, regNum)) {
            DFXLOG_WARN("unable get ip");
            return false;
        }
        if (ip != 0) {
            return true;
        }
    } else {
        DFXLOG_WARN("reg size is 0");
        return false;
    }
    return false;
}

#if HAVE_LIBUNWIND
bool CallStack::DoUnwind(const VirtualThread &thread, std::vector<CallFrame> &callStack,
                         size_t maxStackLevel)
{
    unw_addr_space_t addr_space;
    UnwindInfo unwindInfo = {
        .thread = thread,
        .callStack = *this,
    };
    unw_cursor_t c;
    if (unwindAddrSpaceMap_.count(thread.tid_) == 0) {
        addr_space = unw_create_addr_space(&accessors_, 0);
        if (!addr_space) {
            DFXLOG_ERROR("Can't create unwind vaddress space.");
            return false;
        }
        unwindAddrSpaceMap_.emplace(thread.tid_, addr_space);
        unw_set_caching_policy(addr_space, UNW_CACHE_GLOBAL);
        unw_flush_cache(addr_space, 0, 0);
    } else {
        addr_space = unwindAddrSpaceMap_.at(thread.tid_);
    }

    int ret = unw_init_remote(&c, addr_space, &unwindInfo);
    if (ret) {
        DFXLOG_ERROR("unwind error %d:%s see unw_error_t.", ret, GetUnwErrorName(ret).c_str());
        return false;
    } else {
        UnwindStep(c, callStack, maxStackLevel);
    }
    return true;
}
#endif

bool CallStack::UnwindCallStack(const VirtualThread &thread, bool abi32, u64 *regs, u64 regsNum,
                                const u8 *stack, u64 stackSize, std::vector<CallFrame> &callStack,
                                size_t maxStackLevel)
{
    regs_ = regs;
    regsNum_ = regsNum;
    stack_ = stack;
    stackSize_ = stackSize;

    arch_ = GetArchTypeFromABI(abi32);
    UpdateRegForABI(arch_, regs_);
    if (!RegisterGetSPValue(stackPoint_, arch_, regs_, regsNum_)) {
        DFXLOG_ERROR("RegisterGetSPValue failed");
        return false;
    } else {
        stackEnd_ = stackPoint_ + stackSize_;
    }

#if HAVE_LIBUNWIND
    uint64_t ip;
    uint64_t sp;
    if (!GetIpSP(ip, sp, regs_, regsNum_)) {
        DFXLOG_WARN("unable get sp or sp , unable unwind");
        return false;
    } else {
        if (ip != 0) {
            DFXLOG_DEBUG("unwind:%zu: ip 0x%" PRIx64 " sp 0x%" PRIx64 "", callStack.size(), ip, sp);
            callStack.emplace_back(ip, sp);
        }
    }

    /*
     * If we need more than one entry, do the DWARF
     * unwind itself.
     */
    if (maxStackLevel - 1 > 0) {
        return DoUnwind(thread, callStack, maxStackLevel);
    }
#endif
    return true;
}

bool CallStack::UnwindCallStack(const VirtualThread &thread, u64 *regs, u64 regsNum,
                                const u8 *stack, u64 stackSize, std::vector<CallFrame> &callStack,
                                size_t maxStackLevel)
{
    regs_ = regs;
    regsNum_ = regsNum;
    stack_ = stack;
    stackSize_ = stackSize;

    arch_ = buildArchType;
    UpdateRegForABI(arch_, regs_);
    if (!RegisterGetSPValue(stackPoint_, arch_, regs_, regsNum_)) {
        DFXLOG_ERROR("RegisterGetSPValue failed");
        return false;
    } else {
        stackEnd_ = stackPoint_ + stackSize_;
    }

#if HAVE_LIBUNWIND
    uint64_t ip;
    uint64_t sp;
    if (!GetIpSP(ip, sp, regs_, regsNum_)) {
        DFXLOG_WARN("unable get sp or sp , unable unwind");
        return false;
    } else {
        if (ip != 0) {
            DFXLOG_DEBUG("unwind:%zu: ip 0x%" PRIx64 " sp 0x%" PRIx64 "", callStack.size(), ip, sp);
            callStack.emplace_back(ip, sp);
        }
    }

    /*
     * If we need more than one entry, do the DWARF
     * unwind itself.
     */
    if (maxStackLevel - 1 > 0) {
        return DoUnwind(thread, callStack, maxStackLevel);
    }
#endif
    return true;
}

void CallStack::LogFrame(const std::string msg, const std::vector<CallFrame> &frames)
{
    DFXLOG_DEBUG("%s", msg.c_str());
    int level = 0;
    for (auto frame : frames) {
        DFXLOG_INFO("%d:%s", level++, frame.ToString().c_str());
    }
}

/*
we should have CallStack cache for each thread
end                    begin
0. A -> B -> C -> E -> F
1.           C -> E -> F
2.      B -> C
3. A -> B -> C
4.      B -> G -> H
5.      J -> C

0 is our cache
1 2 3... is from record

use expandLimit to setup how may frame match is needs

*/
size_t CallStack::DoExpandCallStack(std::vector<CallFrame> &newCallFrames,
                                    const std::vector<CallFrame> &cachedCallFrames,
                                    size_t expandLimit)
{
    int maxCycle = 0;

    if (expandLimit == 0 or newCallFrames.size() < expandLimit or
        cachedCallFrames.size() < expandLimit) {
        DFXLOG_INFO("expandLimit %zu not match new %zu cache %zu", expandLimit, newCallFrames.size(),
            cachedCallFrames.size());
        return 0; // size not enough
    }

    // called (Stack Buttom) , this will NOT change when compare
    // in case1 newIt -> C
    // in case2 newIt -> B
    const auto newIt = newCallFrames.end() - expandLimit;

    DFXLOG_INFO("try find new call chain bottom %s for limit %zu", newIt->ToString().c_str(), expandLimit);

    // first frame search, from called - > caller
    // for case 2 it should found B
    size_t distances = expandLimit - 1;
    auto cachedIt = find(cachedCallFrames.begin(), cachedCallFrames.end(), *newIt);
    if (cachedIt == cachedCallFrames.end()) {
        DFXLOG_INFO("not found in first search");
    }

    // cache frame found
    while (std::distance(cachedIt, cachedCallFrames.end()) >= signed(expandLimit)) {
        LOG_ASSERT_MESSAGE(maxCycle++ < MAX_CALL_FRAME_EXPAND_CYCLE, "MAX_UNWIND_CYCLE = %d reach",
                           MAX_CALL_FRAME_EXPAND_CYCLE);

        if (std::equal(newIt, newIt + expandLimit, cachedIt)) {
            DFXLOG_INFO("match %s + %zu", newIt->ToString().c_str(), expandLimit);
            cachedIt += expandLimit; // in while we check the boundary safe
            if (cachedIt == cachedCallFrames.end()) {
                // same but no more need expand
                break;
            }

            // expand the frame and make some log ?
            LogFrame("newCallStack:", newCallFrames);
            LogFrame("cachedCallStack:", cachedCallFrames);

            newCallFrames.insert(newCallFrames.end(), cachedIt, cachedCallFrames.end());
            auto expands = std::distance(cachedIt, cachedCallFrames.end());
            DFXLOG_INFO("merge callstack increse to %zu (+%zd) ", newCallFrames.size(), expands);
            // we done the deal
            return expands;
        } else {
            // quick search next same farme again
            cachedIt++;
            if (cachedIt != cachedCallFrames.end()) {
                DFXLOG_INFO("search next");
                cachedIt = find(cachedIt, cachedCallFrames.end(), *newIt);
            }
        }
    }
    DFXLOG_INFO("cachedIt distance %zd , need %zd", std::distance(cachedCallFrames.begin(), cachedIt), distances);
    return 0u; // nothing expand
}

size_t CallStack::ExpandCallStack(pid_t tid, std::vector<CallFrame> &callFrames, size_t expandLimit)
{
    size_t expand = 0u;
    if (expandLimit == 0) {
        return expand; // nothing need to do
    } else if (callFrames.size() < expandLimit) {
        DFXLOG_INFO("new callstack is too small, skip it");
        return expand;
    }
    if (!cachedCallFramesMap_.count(tid)) {
        cachedCallFramesMap_[tid].reserve(MAX_CALL_FRAME_EXPAND_CACHE_SIZE);
    }
    if (callFrames.size() >= 1u) {
        // get top  (Earliest caller)
        HashList<uint64_t, std::vector<CallFrame>> &cachedCallFrames = cachedCallFramesMap_[tid];
        DFXLOG_INFO("find call stack frames in cache size %zu", cachedCallFrames.size());
        // compare
        using namespace std::rel_ops; // enable complement comparing operators
        for (auto itr = cachedCallFrames.begin(); itr < cachedCallFrames.end(); ++itr) {
            // each cached callstack
            /*
                stack 2    1    0
                cache A -> B -> C
                new        B -> C
                check:
                1 if new B == cache C
                2 if new B == cache B
                3 if new C == new C (if limit > 0)
                4 insert A after B in new stack
            */
            const std::vector<CallFrame> &cachedCallStack = *itr;
            if (cachedCallStack.size() < expandLimit) {
                DFXLOG_INFO("cache callstack is too small, skip it");
                continue; // check next
            }
            expand = DoExpandCallStack(callFrames, cachedCallStack, expandLimit);
            if (expand > 0) {
                break;
            }
        }
        // add new one in to cache cachedCallFrames.
        // further optimization can be done by caching pointer which avoids copying
        // vector
        cachedCallFrames[callFrames[0].ip_] = callFrames;
    }
    DFXLOG_INFO("expand %zu", expand);
    return expand;
}

CallStack::CallStack() {}

CallStack::~CallStack()
{
#if HAVE_LIBUNWIND
    for (auto &pair : unwindAddrSpaceMap_) {
        unw_destroy_addr_space(pair.second);
    }
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
