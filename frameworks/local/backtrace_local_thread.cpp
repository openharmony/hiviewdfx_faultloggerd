/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "backtrace_local_thread.h"

#include <sstream>

#include <link.h>
#include <unistd.h>

#include <libunwind.h>
#include <libunwind_i-ohos.h>
#include <securec.h>

#include "backtrace_local_static.h"
#include "dfx_symbols_cache.h"
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxBacktraceLocal"
constexpr int32_t MIN_VALID_FRAME_COUNT = 3;
}

BacktraceLocalThread::BacktraceLocalThread(int32_t tid) : tid_(tid)
{
#ifdef __aarch64__
    (void)pipe(pipefd_);
#endif
}

BacktraceLocalThread::~BacktraceLocalThread()
{
#ifdef __aarch64__
    if (pipefd_[0] >= 0) {
        close(pipefd_[0]);
        pipefd_[0] = -1;
    }

    if (pipefd_[1] >= 0) {
        close(pipefd_[1]);
        pipefd_[1] = -1;
    }
#endif
    if (tid_ != BACKTRACE_CURRENT_THREAD) {
        BacktraceLocalStatic::GetInstance().CleanUp();
    }
}

void BacktraceLocalThread::UpdateFrameFuncName(unw_addr_space_t as,
    std::shared_ptr<DfxSymbolsCache> cache, NativeFrame& frame)
{
    if (cache != nullptr) {
        cache->GetNameAndOffsetByPc(as, frame.pc, frame.funcName, frame.funcOffset);
    }
}

bool BacktraceLocalThread::UnwindWithContext(unw_addr_space_t as, unw_context_t& context,
    std::shared_ptr<DfxSymbolsCache> cache, size_t skipFrameNum)
{
    unw_cursor_t cursor;
    unw_init_local_with_as(as, &cursor, &context);
    size_t index = 0;
    unw_word_t prevPc = 0;
    do {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFrameNum) {
            index++;
            continue;
        }

        NativeFrame frame;
        frame.index = index - skipFrameNum;
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(frame.pc)))) {
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(frame.sp)))) {
            break;
        }

        if (frame.index > 1 && prevPc == frame.pc) {
            break;
        }
        prevPc = frame.pc;

        frame.relativePc = unw_get_rel_pc(&cursor);
        unw_word_t sz = unw_get_previous_instr_sz(&cursor);
        if ((index > 0) && (frame.relativePc > sz)) {
            frame.relativePc -= sz;
            frame.pc -= sz;
#if defined(__arm__)
            unw_set_adjust_pc(&cursor, frame.pc);
#endif
        }

        struct map_info* map = unw_get_map(&cursor);
        bool isValidFrame = true;
        if ((map != NULL) && (strlen(map->path) < SYMBOL_BUF_SIZE - 1)) {
            frame.binaryName = std::string(map->path);
            UpdateFrameFuncName(as, cache, frame);
        } else {
            isValidFrame = false;
        }

        if (frame.index < MIN_VALID_FRAME_COUNT || isValidFrame) {
            frames_.push_back(frame);
        } else {
            break;
        }

        index++;
    } while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS));
    return frames_.size() > 0;
}

bool BacktraceLocalThread::UnwindCurrentThread(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache,
    size_t skipFrameNum)
{
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);
    return UnwindWithContext(as, context, cache, skipFrameNum + 1);
}

bool BacktraceLocalThread::Unwind(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache,
    size_t skipFrameNum, bool releaseThread)
{
    if (as == nullptr) {
        return false;
    }

    if (tid_ == BACKTRACE_CURRENT_THREAD) {
        return UnwindCurrentThread(as, cache, skipFrameNum + 1);
    } else if (tid_ < BACKTRACE_CURRENT_THREAD) {
        return false;
    }

    auto threadContext = BacktraceLocalStatic::GetInstance().GetThreadContext(tid_);
    if (threadContext == nullptr) {
        return false;
    }

    if (threadContext->ctx == nullptr) {
        // should never happen
        BacktraceLocalStatic::GetInstance().ReleaseThread(tid_);
        return false;
    }

    bool ret = UnwindWithContext(as, *(threadContext->ctx), cache, skipFrameNum);

    if (releaseThread) {
        ReleaseThread();
    }
    return ret;
}

const std::vector<NativeFrame>& BacktraceLocalThread::GetFrames() const
{
    return frames_;
}

std::string BacktraceLocalThread::GetFramesStr()
{
    std::ostringstream ss;
    for (const auto& frame : frames_) {
        ss << GetNativeFrameStr(frame);
    }
    return ss.str();
}

std::string BacktraceLocalThread::GetNativeFrameStr(const NativeFrame& frame)
{
    char buf[1024] = "\0"; // 1024 buffer length
#ifdef __LP64__
    char format[] = "#%02zu pc %016" PRIx64 " %s";
#else
    char format[] = "#%02zu pc %08" PRIx64 " %s";
#endif
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format,
        frame.index,
        frame.relativePc,
        frame.binaryName.empty() ? "Unknown" : frame.binaryName.c_str()) <= 0) {
        return "[Unknown]";
    }

    std::ostringstream ss;
    ss << std::string(buf, strlen(buf));
    if (frame.funcName.empty()) {
        ss << std::endl;
    } else {
        ss << "(";
        ss << frame.funcName.c_str();
        ss << "+" << frame.funcOffset << ")" << std::endl;
    }
    return ss.str();
}

bool BacktraceLocalThread::GetBacktraceFrames(int32_t tid, size_t skipFrameNum, std::vector<NativeFrame>& frames)
{
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    unw_addr_space_t as;
    unw_init_local_address_space(&as);
    if (as == nullptr) {
        return false;
    }
    auto cache = std::make_shared<DfxSymbolsCache>();

    BacktraceLocalThread thread(tid);
    if (!thread.Unwind(as, cache, skipFrameNum)) {
        return false;
    }

    frames.clear();
    frames = thread.GetFrames();

    unw_destroy_local_address_space(as);
    return true;
}

bool BacktraceLocalThread::GetBacktraceString(int32_t tid, size_t skipFrameNum, std::string& out)
{
    std::vector<NativeFrame> frames;
    if (tid == BACKTRACE_CURRENT_THREAD) {
        skipFrameNum += 1;
    }
    bool ret = GetBacktraceFrames(tid, skipFrameNum, frames);
    if (!ret) {
        return false;
    }

    std::stringstream ss;
    for (auto const& frame : frames) {
        ss << GetNativeFrameStr(frame);
    }
    out = ss.str();
    return ret;
}

void BacktraceLocalThread::ReleaseThread()
{
    if (tid_ > BACKTRACE_CURRENT_THREAD) {
        BacktraceLocalStatic::GetInstance().ReleaseThread(tid_);
    }
}

#ifdef __aarch64__
bool BacktraceLocalThread::Step(uintptr_t& fp, uintptr_t& pc)
{
    uintptr_t prevFp = fp;
    if (IsAddressReadable(prevFp) && IsAddressReadable(prevFp + sizeof(uintptr_t))) {
        fp = *reinterpret_cast<uintptr_t*>(prevFp);
        pc = *reinterpret_cast<uintptr_t*>(prevFp + sizeof(uintptr_t));
        return true;
    }

    return false;
}

int BacktraceLocalThread::DlIteratePhdrCallback(struct dl_phdr_info *info, size_t size, void *data)
{
    auto frame = static_cast<NativeFrame*>(data);
    const Elf_W(Phdr) *phdr = info->dlpi_phdr;
    for (int n = info->dlpi_phnum; --n >= 0; phdr++) {
        if (phdr->p_type == PT_LOAD) {
            Elf_W(Addr) vaddr = phdr->p_vaddr + info->dlpi_addr;
            if (frame->pc >= vaddr && frame->pc < vaddr + phdr->p_memsz) {
                frame->relativePc = frame->pc - info->dlpi_addr;
                frame->binaryName = std::string(info->dlpi_name);
                return 1; // let dl_iterate_phdr break
            }
        }
    }
    return 0;
}

bool BacktraceLocalThread::UnwindWithContextByFramePointer(unw_context_t& context, size_t skipFrameNum)
{
    uintptr_t fp = context.uc_mcontext.regs[29]; // 29 : fp location
    uintptr_t pc = context.uc_mcontext.pc;

    int index = 0;
    do {
        if (index < skipFrameNum) {
            index++;
            continue;
        }

        NativeFrame frame;
        frame.index = index - skipFrameNum;
        frame.pc = index == 0 ? pc : pc - 4; // 4 : aarch64 instruction size
        frame.fp = fp;
        frames_.push_back(frame);
        index++;
    } while (Step(fp, pc) && (index < BACK_STACK_MAX_STEPS));
    return frames_.size() > 0;
}

void BacktraceLocalThread::UpdateFrameInfo()
{
    auto it = frames_.begin();
    while (it != frames_.end()) {
        if (dl_iterate_phdr(BacktraceLocalThread::DlIteratePhdrCallback, &(*it)) != 1) {
            // clean up frames after first invalid frame
            frames_.erase(it, frames_.end());
            break;
        }
        it++;
    }
}

bool BacktraceLocalThread::IsAddressReadable(uintptr_t address)
{
    if (pipefd_[1] < 0) {
        return false;
    }

    if (write(pipefd_[1], reinterpret_cast<const void*>(address), 1) < 0) {
        return false;
    }
    return true;
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
