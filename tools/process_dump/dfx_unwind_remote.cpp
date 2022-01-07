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

/* This files real do unwind. */

#include "dfx_unwind_remote.h"

#include <elf.h>
#include <link.h>
#include <cstdio>
#include <cstring>
#include <sys/ptrace.h>

#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "dfx_util.h"

static const int SYMBOL_BUF_SIZE = 4096;
static const int BACK_STACK_MAX_STEPS = 64;  // max unwind 64 steps.

namespace OHOS {
namespace HiviewDFX {
DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<DfxProcess> process)
{
    DfxLogInfo("Enter %s.", __func__);
    if (!process) {
        return false;
    }

    for (auto thread : process->GetThreads()) {
        if (!UnwindThread(process, thread)) {
            DfxLogWarn("Fail to unwind thread.");
        }
    }
    DfxLogInfo("Exit %s.", __func__);
    return true;
}

bool DfxUnwindRemote::DfxUnwindRemoteDoUnwindStep(size_t const & index,
    std::shared_ptr<DfxThread> & thread, unw_cursor_t & cursor, std::shared_ptr<DfxProcess> process)
{
    DfxLogInfo("Enter %s.", __func__);
    std::shared_ptr<DfxFrames> frame = thread->GetAvaliableFrame();
    if (!frame) {
        DfxLogWarn("Fail to create Frame.");
        return false;
    }

    frame->SetFrameIndex(index);
    std::string strSym;
    uint64_t framePc = frame->GetFramePc();
    if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&framePc))) {
        DfxLogWarn("Fail to get program counter.");
        return false;
    }
    frame->SetFramePc(framePc);

    uint64_t frameSp = frame->GetFrameSp();
    if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&frameSp))) {
        DfxLogWarn("Fail to get stack pointer.");
        return false;
    }
    frame->SetFrameSp(frameSp);

    std::shared_ptr<DfxElfMaps> processMaps = process->GetMaps();
    if (!processMaps) {
        DfxLogWarn("Fail to get processMaps pointer.");
        return false;
    }
    auto frameMap = frame->GetFrameMap();
    if (processMaps->FindMapByAddr(frame->GetFramePc(), frameMap)) {
        frame->SetFrameRelativePc(frame->GetRelativePc(process->GetMaps()));
    }

    char sym[SYMBOL_BUF_SIZE] = {0};
    uint64_t frameOffset = frame->GetFrameFuncOffset();
    if (unw_get_proc_name(&cursor,
        sym,
        SYMBOL_BUF_SIZE,
        (unw_word_t*)(&frameOffset)) == 0) {
        std::string funcName;
        std::string strSym(sym, sym + strlen(sym));
        TrimAndDupStr(strSym, funcName);
        frame->SetFrameFuncName(funcName);
        frame->SetFrameFuncOffset(frameOffset);
    }
    DfxLogInfo("Exit %s.", __func__);
    return true;
}

bool DfxUnwindRemote::UnwindThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    DfxLogInfo("Enter %s.", __func__);
    if (!thread) {
        DfxLogWarn("NULL thread needs unwind.");
        return false;
    }

    unw_addr_space_t as = unw_create_addr_space(&_UPT_accessors, 0);
    if (!as) {
        return false;
    }

    pid_t tid = thread->GetThreadId();
    void *context = _UPT_create(tid);
    if (!context) {
        unw_destroy_addr_space(as);
        return false;
    }

    unw_cursor_t cursor;
    if (unw_init_remote(&cursor, as, context) != 0) {
        DfxLogWarn("Fail to init cursor for remote unwind.");
        _UPT_destroy(context);
        unw_destroy_addr_space(as);
        return false;
    }

    size_t index = 0;
    do {
        if (!DfxUnwindRemoteDoUnwindStep(index, thread, cursor, process)) {
            break;
        }
        index++;
    } while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS));
    _UPT_destroy(context);
    unw_destroy_addr_space(as);
    DfxLogInfo("Exit %s.", __func__);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
