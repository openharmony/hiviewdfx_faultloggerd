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

#include <cstdio>
#include <cstring>
#include <elf.h>
#include <link.h>
#include <securec.h>
#include <sys/ptrace.h>
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_symbols_cache.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "libunwind.h"
#include "libunwind_i-ohos.h"

namespace OHOS {
namespace HiviewDFX {
// we should have at least 2 frames, one is pc and the other is lr
// if pc and lr are both invalid, just try fp
static const int MIN_VALID_FRAME_COUNT = 3;

DfxUnwindRemote &DfxUnwindRemote::GetInstance()
{
    static DfxUnwindRemote ins;
    return ins;
}

DfxUnwindRemote::DfxUnwindRemote()
{
    as_ = nullptr;
    std::unique_ptr<DfxSymbolsCache> cache(new DfxSymbolsCache());
    cache_ = std::move(cache);
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<DfxProcess> process)
{
    if (!process) {
        return false;
    }

    auto threads = process->GetThreads();
    DfxLogDebug("%s :: threads(%d)", __func__, threads.size());
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
    if (!process->GetIsSignalDump() && \
        !DfxConfig::GetInstance().GetDumpOtherThreads()) {
        bool ret = UnwindThread(process, threads[0]);
        if (threads[0]->GetIsCrashThread() && (process->GetIsSignalDump() == false)) {
            process->PrintProcessMapsByConfig();
        }
        PrintBuildIds();
        unw_destroy_addr_space(as_);
        as_ = nullptr;
        return ret;
    }

    size_t index = 0;
    for (auto thread : threads) {
        if (index == 1) {
            process->PrintThreadsHeaderByConfig();
        }

        if (thread->Attach()) {
            UnwindThread(process, thread);
            thread->Detach();
        }

        if (thread->GetIsCrashThread() && (process->GetIsSignalDump() == false)) {
            process->PrintProcessMapsByConfig();
        }
        index++;
        DfxLogDebug("%s :: index(%d)", __func__, index);
    }

    PrintBuildIds();
    unw_destroy_addr_space(as_);
    as_ = nullptr;
    return true;
}

uint64_t DfxUnwindRemote::DfxUnwindRemoteDoAdjustPc(unw_cursor_t & cursor, uint64_t pc)
{
    DfxLogDebug("%s :: pc(0x%x).", __func__, pc);

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

    DfxLogDebug("%s :: ret(0x%x).", __func__, ret);
    return ret;
}

std::string DfxUnwindRemote::GetReadableBuildId(uint8_t* buildId, size_t length)
{
    constexpr size_t MAX_BUILD_ID_LENGTH = 32;
    if (length > MAX_BUILD_ID_LENGTH) {
        std::string ret = "Wrong Build-Id length:" + std::to_string(length);
        return ret;
    }

    static const char hexTable[] = "0123456789abcdef";
    uint8_t* buildIdPtr = buildId;
    std::string buildIdStr;
    for (size_t i = 0; i < length; i++) {
        buildIdStr.push_back(hexTable[*buildIdPtr >> 4]); // 4 : higher 4 bit of uint8
        buildIdStr.push_back(hexTable[*buildIdPtr & 0xf]);
        buildIdPtr++;
    }
    return buildIdStr;
}

bool DfxUnwindRemote::DfxUnwindRemoteDoUnwindStep(size_t const & index,
    std::shared_ptr<DfxThread> & thread, unw_cursor_t & cursor, std::shared_ptr<DfxProcess> process)
{
    std::shared_ptr<DfxFrame> frame = thread->GetAvaliableFrame();
    if (!frame) {
        DfxLogWarn("Fail to create Frame.");
        return false;
    }

    frame->SetFrameIndex(index);
    std::string strSym;
    uint64_t framePc = frame->GetFramePc();
    static unw_word_t oldPc = 0;
    if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&framePc))) {
        DfxLogWarn("Fail to get program counter.");
        return false;
    }
    if (oldPc == framePc && index != 0) {
        return false;
    }
    oldPc = framePc;
    frame->SetFramePc(framePc);

    uint64_t frameSp = frame->GetFrameSp();
    if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&frameSp))) {
        DfxLogWarn("Fail to get stack pointer.");
        return false;
    }
    frame->SetFrameSp(frameSp);

    uint64_t relPc = unw_get_rel_pc(&cursor);
    if (relPc == 0) {
        relPc = framePc;
    }

    if (index != 0) {
        relPc = DfxUnwindRemoteDoAdjustPc(cursor, relPc);
    }
    frame->SetFrameRelativePc(relPc);
    bool ret = UpdateAndPrintFrameInfo(cursor, thread, frame,
        (thread->GetIsCrashThread() && !process->GetIsSignalDump()));
    DfxLogDebug("%s :: index(%d), framePc(0x%x), frameSp(0x%x).", __func__, index, framePc, frameSp);
    return ret;
}

bool DfxUnwindRemote::UpdateAndPrintFrameInfo(unw_cursor_t& cursor, std::shared_ptr<DfxThread> thread,
    std::shared_ptr<DfxFrame> frame, bool enableBuildId)
{
    struct map_info* mapInfo = unw_get_map(&cursor);
    bool isValidFrame = true;
    if (mapInfo != nullptr) {
        std::string mapPath = std::string(mapInfo->path);
        frame->SetFrameMapName(mapPath);
        std::string funcName;
        uint64_t funcOffset;
        if (cache_->GetNameAndOffsetByPc(as_, frame->GetFramePc(), funcName, funcOffset)) {
            frame->SetFrameFuncName(funcName);
            frame->SetFrameFuncOffset(funcOffset);
        }

        if (enableBuildId && (buildIds_.find(mapPath) == buildIds_.end())) {
            uint8_t* buildId = nullptr;
            size_t length = 0;
            if (unw_get_build_id(mapInfo, &buildId, &length)) {
                buildIds_.insert(std::pair<std::string, std::string>(std::string(mapPath),
                    GetReadableBuildId(buildId, length)));
            } else {
                buildIds_.insert(std::pair<std::string, std::string>(std::string(mapPath), "No GNU BuildId"));
            }
        }
    } else {
        std::string tips = "Not mapped ";
        std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
        if (regs != nullptr) {
            tips.append(regs->GetSpecialRegisterName(frame->GetFramePc()));
        }
        frame->SetFrameMapName(tips);
        isValidFrame = false;
    }

    bool ret = frame->GetFrameIndex() < MIN_VALID_FRAME_COUNT || isValidFrame;
    if (ret) {
        DfxRingBufferWrapper::GetInstance().AppendMsg(frame->PrintFrame());
    }
    return ret;
}

void DfxUnwindRemote::PrintBuildIds() const
{
    if (buildIds_.size() == 0) {
        return;
    }

    DfxRingBufferWrapper::GetInstance().AppendMsg("Related elf build-id:\n");
    for (auto const& buildId : buildIds_) {
        std::string line = buildId.first + ":" + buildId.second + "\n";
        DfxRingBufferWrapper::GetInstance().AppendMsg(line);
    }
}

bool DfxUnwindRemote::UnwindThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    if (!thread) {
        DfxLogWarn("NULL thread needs unwind.");
        return false;
    }

    pid_t tid = thread->GetThreadId();
    char buf[LOG_BUF_LEN] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Tid:%d, Name:%s\n", tid, thread->GetThreadName().c_str());
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    DfxRingBufferWrapper::GetInstance().AppendMsg(std::string(buf));

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

    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
    if (regs != nullptr) {
        std::vector<uintptr_t> regsVector = regs->GetRegsData();
        unw_set_context(&cursor, regsVector.data(), regsVector.size());
    }

    size_t index = 0;
    int unwRet = 0;
    do {
        // store current frame
        if (!DfxUnwindRemoteDoUnwindStep(index, thread, cursor, process)) {
            break;
        }
        index++;

        // find next frame
        unwRet = unw_step(&cursor);
    } while ((unwRet > 0) && (index < BACK_STACK_MAX_STEPS));
    thread->SetThreadUnwStopReason(unwRet);
    if (thread->GetIsCrashThread() && (process->GetIsSignalDump() == false)) {
        DfxRingBufferWrapper::GetInstance().AppendMsg(regs->PrintRegs());
        if (DfxConfig::GetInstance().GetDisplayFaultStack()) {
            thread->CreateFaultStack();
            thread->PrintThreadFaultStackByConfig();
        }
    }
    _UPT_destroy(context);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
