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

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "dfx_util.h"

static const int SYMBOL_BUF_SIZE = 4096;

namespace OHOS {
namespace HiviewDFX {
DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<DfxProcess> process)
{
    DfxLogDebug("Enter %s.", __func__);
    if (!process) {
        return false;
    }

    for (auto thread : process->GetThreads()) {
        if (!UnwindThread(process, thread)) {
            DfxLogWarn("Fail to unwind thread.");
        }
    }
    DfxLogDebug("Exit %s.", __func__);
    return true;
}

uint64_t DfxUnwindRemote::DfxUnwindRemoteDoAdjustPc(uint64_t pc)
{
    DfxLogDebug("Enter %s :: pc(0x%x).", __func__, pc);

    uint64_t ret = 0;

    if (pc == 0) {
        ret = pc; // pc zero is abnormal case, so we don't adjust pc.
    } else {
#if defined(__arm__)
        if (pc & 1) { // thumb mode, pc step is 2 byte.
            ret = pc - ARM_EXEC_STEP_THUMB;
        } else {
            ret = pc - ARM_EXEC_STEP_NORMAL;
        }
#elif defined(__aarch64__)
        ret = pc - ARM_EXEC_STEP_NORMAL;
#endif
    }

    DfxLogDebug("Exit %s :: ret(0x%x).", __func__, ret);
    return ret;
}

bool DfxUnwindRemote::DfxUnwindRemoteDoUnwindStep(size_t const & index,
    std::shared_ptr<DfxThread> & thread, unw_cursor_t & cursor, std::shared_ptr<DfxProcess> process)
{
    DfxLogDebug("Enter %s :: index(%d).", __func__, index);
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

    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
    if (regs != NULL) {
        std::vector<uintptr_t> regsVector = regs->GetRegsData();
        if (regsVector[REG_PC_NUM] != framePc) {
            framePc = DfxUnwindRemoteDoAdjustPc(framePc);
        }
    }
    frame->SetFramePc(framePc);

    uint64_t frameLr = frame->GetFrameLr();
    if (unw_get_reg(&cursor, REG_LR_NUM, (unw_word_t*)(&frameLr))) {
        DfxLogWarn("Fail to get program counter.");
        return false;
    } else {
        frame->SetFrameLr(frameLr);
    }
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
    DfxLogDebug("Exit %s :: index(%d), framePc(0x%x), frameSp(0x%x).", __func__, index, framePc, frameSp);
    return true;
}

bool DfxUnwindRemote::UnwindThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    DfxLogDebug("Enter %s.", __func__);
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
    int unwRet = 0;
    unw_word_t old_pc = 0;
    do {
        unw_word_t tmpPc = 0;
        unw_get_reg(&cursor, UNW_REG_IP, &tmpPc);

        // Exit unwind step as pc has no change. -S-
        if (old_pc == tmpPc) {
            DfxLogWarn("Break unwstep as tmpPc is same with old_ip .");
            break;
        }
        old_pc = tmpPc;
        // Exit unwind step as pc has no change. -E-

        if (!DfxUnwindRemoteDoUnwindStep(index, thread, cursor, process)) {
            DfxLogWarn("Break unwstep as DfxUnwindRemoteDoUnwindStep failed.");
            break;
        }
        index++;

        // Add to check pc is valid in maps x segment, if check failed use lr to backtrace instead -S-.
        std::shared_ptr<DfxElfMaps> processMaps = process->GetMaps();
        if (!processMaps->CheckPcIsValid((uint64_t)tmpPc)) {
            unw_get_reg(&cursor, REG_LR_NUM, &tmpPc);
            unw_set_reg(&cursor, UNW_REG_IP, tmpPc);
            if (!DfxUnwindRemoteDoUnwindStep(index, thread, cursor, process)) {  // Add lr frame to frame list.
                DfxLogWarn("Break unwstep as DfxUnwindRemoteDoUnwindStep failed.");
                break;
            }
            index++;
        }
        // Add to check pc is valid in maps x segment, if check failed use lr to backtrace instead -E-.
        unwRet = unw_step(&cursor);
        // if we use context's pc unwind failed, try lr -S-.
        if (unwRet <= 0) {
            std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
            if (regs != NULL) {
                std::vector<uintptr_t> regsVector = regs->GetRegsData();
                if (regsVector[REG_PC_NUM] == old_pc) {
                    unw_set_reg(&cursor, UNW_REG_IP, regsVector[REG_LR_NUM]);
                    if (!DfxUnwindRemoteDoUnwindStep(index, thread, cursor, process)) {  // Add lr frame to frame list.
                        DfxLogWarn("Break unwstep as DfxUnwindRemoteDoUnwindStep failed.");
                        break;
                    }
                    index++;
                    unwRet = unw_step(&cursor);
                }
            }
        }
        // if we use context's pc unwind failed, try lr -E-.
    } while ((unwRet > 0) && (index < BACK_STACK_MAX_STEPS));
    thread->SetThreadUnwStopReason(unwRet);
    _UPT_destroy(context);
    unw_destroy_addr_space(as);
    bool isSignal = process->GetIsSignalHdlr();
    if (((*(process->GetThreads().begin()))->GetThreadId() == tid) && isSignal) {
        (*(process->GetThreads().begin()))->SkipFramesInSignalHandler();
        if (process->GetIsSignalDump() == false) {
            std::shared_ptr<DfxElfMaps> maps = process->GetMaps();
            thread->CreateFaultStack(maps);
        }
    }

    DfxLogDebug("Exit %s.", __func__);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
