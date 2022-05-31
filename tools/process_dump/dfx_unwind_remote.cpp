/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <securec.h>

#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "process_dumper.h"
#include "dfx_symbols_cache.h"

#include "libunwind.h"
#include "libunwind_i-ohos.h"

namespace OHOS {
namespace HiviewDFX {
static const int SYMBOL_BUF_SIZE = 4096;
static const int INVALID_FRAME_COUNT = 2;

DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

DfxUnwindRemote::DfxUnwindRemote()
{
    as_ = nullptr;
    cache_ = std::make_unique<DfxSymbolsCache>();
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<DfxProcess> process)
{
    if (!process) {
        return false;
    }

    auto threads = process->GetThreads();
    if (threads.empty()) {
        return false;
    }

    as_ = unw_create_addr_space(&_UPT_accessors, 0);
    if (!as_) {
        return false;
    }
    unw_set_target_pid(as_, process->GetPid());
    unw_set_caching_policy(as_, UNW_CACHE_GLOBAL);

    // only need to unwind crash thread in crash scenario
    if (process->GetIsSignalHdlr() && !process->GetIsSignalDump() && \
        !DfxConfig::GetInstance().GetDumpOtherThreads()) {
        bool ret = UnwindThread(process, threads[0]);
        if (threads[0]->GetIsCrashThread() && (process->GetIsSignalDump() == false) && \
            (process->GetIsSignalHdlr() == true)) {
            process->PrintProcessMapsByConfig();
        }
        unw_destroy_addr_space(as_);
        as_ = nullptr;
        return ret;
    }

    size_t index = 0;
    for (auto thread : threads) {
        if (index == 1) {
            process->PrintThreadsHeaderByConfig();
        }

        if (!UnwindThread(process, thread)) {
            DfxLogWarn("Fail to unwind thread.");
        }

        if (thread->GetIsCrashThread() && (process->GetIsSignalDump() == false) && \
            (process->GetIsSignalHdlr() == true)) {
            process->PrintProcessMapsByConfig();
        }
        index++;
    }

    unw_destroy_addr_space(as_);
    as_ = nullptr;
    return true;
}

uint64_t DfxUnwindRemote::DfxUnwindRemoteDoAdjustPc(unw_cursor_t & cursor, uint64_t pc)
{
    DfxLogDebug("Enter %s :: pc(0x%x).", __func__, pc);

    uint64_t ret = 0;

    if (pc <= ARM_EXEC_STEP_NORMAL) {
        ret = pc; // pc zero is abnormal case, so we don't adjust pc.
    } else {
#if defined(__arm__)
        ret = pc - unw_get_previous_instr_sz(&cursor);
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
    bool isSignalHdlr = process->GetIsSignalHdlr();
    if (regs != NULL) {
        std::vector<uintptr_t> regsVector = regs->GetRegsData();
        if (regsVector[REG_PC_NUM] != framePc) {
            framePc = DfxUnwindRemoteDoAdjustPc(cursor, framePc);
        }
    } else {
        if (!isSignalHdlr) {
            framePc = DfxUnwindRemoteDoAdjustPc(cursor, framePc);
        }
    }

    frame->SetFramePc(framePc);

    uint64_t frameLr = frame->GetFrameLr();
    if (unw_get_reg(&cursor, REG_LR_NUM, (unw_word_t*)(&frameLr))) {
        DfxLogWarn("Fail to get lr.");
        frame->SetFrameLr(0);
    } else {
        frame->SetFrameLr(frameLr);
    }
    uint64_t frameSp = frame->GetFrameSp();
    if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&frameSp))) {
        DfxLogWarn("Fail to get stack pointer.");
        return false;
    }
    frame->SetFrameSp(frameSp);

    if (index != 0) {
        frame->SetFrameRelativePc(unw_get_rel_pc(&cursor) - unw_get_previous_instr_sz(&cursor));
    } else {
        // pc frame, so needn't adjust relpc.
        frame->SetFrameRelativePc(unw_get_rel_pc(&cursor));
    }
    struct map_info* mapInfo = unw_get_map(&cursor);
    bool isValidFrame = true;
    if (mapInfo != nullptr) {
        frame->SetFrameMapName(mapInfo->path);
        std::string funcName;
        uint64_t funcOffset;
        if (cache_->GetNameAndOffsetByPc(as_, framePc, funcName, funcOffset)) {
            frame->SetFrameFuncName(funcName);
	    frame->SetFrameFuncOffset(funcOffset);
        }	
    } else {
        isValidFrame = false;    
    }

    OHOS::HiviewDFX::ProcessDumper::GetInstance().PrintDumpProcessMsg(frame->PrintFrame());
 
    DfxLogDebug("Exit %s :: index(%d), framePc(0x%x), frameSp(0x%x).", __func__, index, framePc, frameSp);
    return index < INVALID_FRAME_COUNT || isValidFrame;
}

bool DfxUnwindRemote::UnwindThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    DfxLogDebug("Enter %s.", __func__);
    if (!thread) {
        DfxLogWarn("NULL thread needs unwind.");
        return false;
    }

    pid_t tid = thread->GetThreadId();
    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();

    char buf[LOG_BUF_LEN] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Tid:%d, Name:%s\n", tid, thread->GetThreadName().c_str());
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    OHOS::HiviewDFX::ProcessDumper::GetInstance().PrintDumpProcessMsg(std::string(buf));

    uintptr_t regStorePc = 0;
    uintptr_t regStoreLr = 0;
    uintptr_t regStoreSp = 0;
    if (regs != nullptr) {
        std::vector<uintptr_t> regsVector = regs->GetRegsData();
        regStorePc = regsVector[REG_PC_NUM];
        regStoreLr = regsVector[REG_LR_NUM];
        regStoreSp = regsVector[REG_SP_NUM];
    }

    void *context = _UPT_create(tid);
    if (!context) {
        return false;
    }

    if (!as_) {
        as_ = unw_create_addr_space(&_UPT_accessors, 0);
        if (!as_) {
            return false;
        }
        unw_set_caching_policy(as_, UNW_CACHE_GLOBAL);
    }

    unw_cursor_t cursor;
    if (as_ && unw_init_remote(&cursor, as_, context) != 0) {
        DfxLogWarn("Fail to init cursor for remote unwind.");
        _UPT_destroy(context);
        return false;
    }

    size_t index = 0;
    int unwRet = 0;
    unw_word_t oldPc = 0;
    size_t crashUnwStepPosition = 0;
    size_t skipFrames = 0;
    bool useLrUnwStep = false;
    bool isSigDump = process->GetIsSignalDump();
    do {
        unw_word_t tmpPc = 0;
        unw_get_reg(&cursor, UNW_REG_IP, &tmpPc);
        // Exit unwind step as pc has no change. -S-
        if (oldPc == tmpPc && index != 0) {
            DfxLogWarn("Break unwstep as tmpPc is same with old_ip .");
            break;
        }
        oldPc = tmpPc;
        // Exit unwind step as pc has no change. -E-

        if (thread->GetIsCrashThread() && (regStorePc == tmpPc)) {
            crashUnwStepPosition = index + 1;
            skipFrames = index;
        } else if (thread->GetIsCrashThread() && (regStoreLr == tmpPc) && (regStorePc == 0x0)) {
            // Lr position found in crash thread. We need:
            // 1. mark skipFrames.
            // 2. Add pc zero frame.
            useLrUnwStep = true;
            skipFrames = index;
            std::shared_ptr<DfxFrames> frame = thread->GetAvaliableFrame();
            frame->SetFrameIndex(0);
            frame->SetFramePc(regStorePc);
            frame->SetFrameLr(regStoreLr);
            frame->SetFrameSp(regStoreSp);
            OHOS::HiviewDFX::ProcessDumper::GetInstance().PrintDumpProcessMsg(frame->PrintFrame());
            index++;
        }

        if (skipFrames != 0 || thread->GetIsCrashThread() == false) {
            if (!DfxUnwindRemoteDoUnwindStep((index - skipFrames), thread, cursor, process)) {
                DfxLogWarn("Break unwstep as DfxUnwindRemoteDoUnwindStep failed -1.");
                break;
            }
        }

        index++;

        // Add to check pc is valid in maps x segment, if check failed use lr to backtrace instead -S-.
        std::shared_ptr<DfxElfMaps> processMaps = process->GetMaps();
        if (!isSigDump && !processMaps->CheckPcIsValid((uint64_t)tmpPc) &&
            (crashUnwStepPosition == index)) {
            unw_set_reg(&cursor, UNW_REG_IP, regStoreLr);
            // Add lr frame to frame list.
            if (!DfxUnwindRemoteDoUnwindStep((index - skipFrames), thread, cursor, process)) {
                DfxLogWarn("Break unwstep as DfxUnwindRemoteDoUnwindStep failed -2.");
                break;
            }
            index++;
        }
        // Add to check pc is valid in maps x segment, if check failed use lr to backtrace instead -E-.
        unwRet = unw_step(&cursor);
        // if we use context's pc unwind failed, try lr -S-.
        if (unwRet <= 0) {
            if (!isSigDump && (crashUnwStepPosition == index)) {
                unw_set_reg(&cursor, UNW_REG_IP, regStoreLr);
                // Add lr frame to frame list.
                if (!DfxUnwindRemoteDoUnwindStep((index - skipFrames), thread, cursor, process)) {
                    DfxLogWarn("Break unwstep as DfxUnwindRemoteDoUnwindStep failed -3.");
                    break;
                }
                index++;
                unwRet = unw_step(&cursor);
            }
        }
        // if we use context's pc unwind failed, try lr -E-.
    } while ((unwRet > 0) && (index < BACK_STACK_MAX_STEPS));
    thread->SetThreadUnwStopReason(unwRet);
    _UPT_destroy(context);

    if (process->GetIsSignalHdlr() && thread->GetIsCrashThread() && (process->GetIsSignalDump() == false)) {
        OHOS::HiviewDFX::ProcessDumper::GetInstance().PrintDumpProcessMsg(regs->PrintRegs());
        std::shared_ptr<DfxElfMaps> maps = process->GetMaps();
        if (DfxConfig::GetInstance().GetDisplayFaultStack()) {
            thread->CreateFaultStack(maps);
            OHOS::HiviewDFX::ProcessDumper::GetInstance().PrintDumpProcessMsg(\
                thread->PrintThreadFaultStackByConfig() + "\n");
        }
    }

    DfxLogDebug("Exit %s.", __func__);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
