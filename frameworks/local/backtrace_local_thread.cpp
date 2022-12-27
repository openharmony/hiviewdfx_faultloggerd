/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
}

void BacktraceLocalThread::DoUnwind(unw_addr_space_t as, unw_context_t& context, std::unique_ptr<DfxSymbolsCache>& cache,
    size_t skipFrameNum)
{
    unw_cursor_t cursor;
    unw_init_local_with_as(as, &cursor, &context);
    size_t index = 0;
    unw_word_t prevPc = 0;
    uint64_t offset = 0;
    std::string name;
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
            cache->GetNameAndOffsetByPc(as, frame.pc, frame.funcName, frame.funcOffset);
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
}

void BacktraceLocalThread::DoUnwindCurrent(unw_addr_space_t as, std::unique_ptr<DfxSymbolsCache>& cache, size_t skipFrameNum)
{
    unw_context_t context;
    unw_getcontext(&context);
    DoUnwind(as, context, cache, skipFrameNum + 1);
}

bool BacktraceLocalThread::Unwind(unw_addr_space_t as, std::unique_ptr<DfxSymbolsCache>& cache, size_t skipFrameNum)
{
    
    if (tid_ == BACKTRACE_CURRENT_THREAD) {
        DoUnwindCurrent(as, cache, skipFrameNum + 1);
        return true;
    } else if (tid_ < BACKTRACE_CURRENT_THREAD) {
        return false;
    }

    unw_context_t context;
    if (!BacktraceLocalStatic::GetInstance().GetThreadContext(tid_, context)) {
        return false;
    }

    DoUnwind(as, context, cache, skipFrameNum + 1);

    if (tid_ > BACKTRACE_CURRENT_THREAD) {
        BacktraceLocalStatic::GetInstance().ReleaseThread(tid_);
    }
    return true;
}

const std::vector<NativeFrame>& BacktraceLocalThread::GetFrames() const
{
    return frames_;
}
} // namespace HiviewDFX
} // namespace OHOS
