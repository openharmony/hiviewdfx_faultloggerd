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

#include <algorithm>
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
#include "process_dumper.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
// we should have at least 2 frames, one is pc and the other is lr
// if pc and lr are both invalid, just try fp
static constexpr int MIN_VALID_FRAME_COUNT = 3;
static constexpr int ARM_EXEC_STEP_NORMAL = 4;
static constexpr size_t MAX_BUILD_ID_LENGTH = 32;
static constexpr int ARK_JS_HEAD_LEN = 256;
}

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
    bool ret = false;
    if (!process) {
        DFXLOG_WARN("%s::can not unwind null process.", __func__);
        return ret;
    }

    auto threads = process->GetThreads();
    if (threads.empty()) {
        DFXLOG_WARN("%s::no thread under target process.", __func__);
        return ret;
    }

    as_ = unw_create_addr_space(&_UPT_accessors, 0);
    if (!as_) {
        DFXLOG_WARN("%s::failed to create address space.", __func__);
        return ret;
    }

    unw_set_target_pid(as_, ProcessDumper::GetInstance().GetTargetPid());
    unw_set_caching_policy(as_, UNW_CACHE_GLOBAL);
    do {
        // only need to unwind crash thread in crash scenario
        if (!process->GetIsSignalDump() && !DfxConfig::GetInstance().GetDumpOtherThreads()) {
            ret = UnwindThread(process, threads[0]);
            if (!ret) {
                UnwindThreadFallback(process, threads[0]);
            }

            if (threads[0]->GetIsCrashThread() && (!process->GetIsSignalDump())) {
                process->PrintProcessMapsByConfig();
            }
            break;
        }

        size_t index = 0;
        for (auto thread : threads) {
            if (index == 1) {
                process->PrintThreadsHeaderByConfig();
            }
            if (index != 0) {
                DfxRingBufferWrapper::GetInstance().AppendMsg("\n");
            }

            if (thread->Attach()) {
                UnwindThread(process, thread);
                thread->Detach();
            }

            if (thread->GetIsCrashThread() && (!process->GetIsSignalDump())) {
                process->PrintProcessMapsByConfig();
            }
            index++;
        }
        ret = true;
    } while (false);

    unw_destroy_addr_space(as_);
    as_ = nullptr;
    return ret;
}

uint64_t DfxUnwindRemote::DfxUnwindRemoteDoAdjustPc(unw_cursor_t & cursor, uint64_t pc)
{
    DFXLOG_DEBUG("%s :: pc(0x%x).", __func__, pc);

    uint64_t ret = 0;

    if (pc <= ARM_EXEC_STEP_NORMAL) {
        ret = pc; // pc zero is abnormal case, so we don't adjust pc.
    } else {
#if defined(__arm__)
        ret = pc - unw_get_previous_instr_sz(&cursor);
#elif defined(__aarch64__)
        ret = pc - ARM_EXEC_STEP_NORMAL;
#elif defined(__x86_64__)
        ret = pc - 1;
#endif
    }

    DFXLOG_DEBUG("%s :: ret(0x%x).", __func__, ret);
    return ret;
}

std::string DfxUnwindRemote::GetReadableBuildId(uint8_t* buildId, size_t length)
{
    if (length > MAX_BUILD_ID_LENGTH) {
        std::string ret = "Wrong Build-Id length:" + std::to_string(length);
        return ret;
    }

    static const char HEXTABLE[] = "0123456789abcdef";
    uint8_t* buildIdPtr = buildId;
    std::string buildIdStr;
    for (size_t i = 0; i < length; i++) {
        buildIdStr.push_back(HEXTABLE[*buildIdPtr >> 4]); // 4 : higher 4 bit of uint8
        buildIdStr.push_back(HEXTABLE[*buildIdPtr & 0xf]);
        buildIdPtr++;
    }
    return buildIdStr;
}

bool DfxUnwindRemote::DfxUnwindRemoteDoUnwindStep(size_t const & index,
    std::shared_ptr<DfxThread> & thread, unw_cursor_t & cursor, std::shared_ptr<DfxProcess> process)
{
    bool ret = false;
    uint64_t framePc;
    static unw_word_t oldPc = 0;
    if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&framePc))) {
        DFXLOG_WARN("Fail to get current pc.");
        return ret;
    }

    uint64_t frameSp;
    if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&frameSp))) {
        DFXLOG_WARN("Fail to get stack pointer.");
        return ret;
    }

    if (oldPc == framePc && index != 0) {
        return ret;
    }
    oldPc = framePc;

    uint64_t relPc = unw_get_rel_pc(&cursor);
    if (index != 0) {
        relPc = DfxUnwindRemoteDoAdjustPc(cursor, relPc);
        framePc = DfxUnwindRemoteDoAdjustPc(cursor, framePc);
#if defined(__arm__)
        unw_set_adjust_pc(&cursor, framePc);
#endif
    }

    if (relPc == 0) {
        relPc = framePc;
    }

    auto frame = std::make_shared<DfxFrame>();
    frame->SetFrameRelativePc(relPc);
    frame->SetFramePc(framePc);
    frame->SetFrameSp(frameSp);
    frame->SetFrameIndex(index);
    ret = UpdateAndPrintFrameInfo(cursor, thread, frame,
        (thread->GetIsCrashThread() && !process->GetIsSignalDump()));
    if (ret) {
        thread->AddFrame(frame);
    }

    DFXLOG_DEBUG("%s :: index(%d), framePc(0x%x), frameSp(0x%x).", __func__, index, framePc, frameSp);
    return ret;
}

bool DfxUnwindRemote::GetArkJsHeapFuncName(std::string& funcName, std::shared_ptr<DfxThread> thread)
{
    bool ret = false;
#if defined(__aarch64__)
    char buf[ARK_JS_HEAD_LEN] = {0};
    do {
        std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
        if (regs == nullptr) {
            break;
        }
        std::vector<uintptr_t> regsVector = regs->GetRegsData();
        if (regsVector.size() < (UNW_AARCH64_X29 + 1)) {
            break;
        }
        uintptr_t x20 = regsVector[UNW_AARCH64_X20];
        uintptr_t fp = regsVector[UNW_AARCH64_X29];

        DFXLOG_INFO("pid: %d, x20: %016lx, fp: %016lx", thread->GetThreadId(), x20, fp);
        int result = unw_get_ark_js_heap_crash_info(thread->GetThreadId(),
            (uintptr_t*)&x20, (uintptr_t*)&fp, false, buf, ARK_JS_HEAD_LEN);
        if (result < 0) {
            DFXLOG_WARN("Fail to unw_get_ark_js_heap_crash_info.");
            break;
        }
        ret = true;

        size_t len = std::min(strlen(buf), static_cast<size_t>(ARK_JS_HEAD_LEN - 1));
        funcName = std::string(buf, len);
    } while (false);
#endif
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
        bool isGetFuncName = false;
#if defined(__aarch64__)
        if ((frame->GetFrameIndex() == 0) && ((mapPath.find("ArkTS Code") != std::string::npos))) {
            isGetFuncName = GetArkJsHeapFuncName(funcName, thread);
            if (isGetFuncName) {
                frame->SetFrameFuncName(funcName);
            }
        }
#endif
        uint64_t funcOffset;
        if (!isGetFuncName && cache_->GetNameAndOffsetByPc(as_, frame->GetFramePc(), funcName, funcOffset)) {
            frame->SetFrameFuncName(funcName);
            frame->SetFrameFuncOffset(funcOffset);
        }
        if (enableBuildId && buildIds_.find(mapPath) != buildIds_.end()) {
            frame->SetBuildId(buildIds_[mapPath]);
        } else if (enableBuildId && buildIds_.find(mapPath) == buildIds_.end()) {
            uint8_t* buildId = nullptr;
            size_t length = 0;
            std::string buildIdStr = "";
            if (unw_get_build_id(mapInfo, &buildId, &length)) {
                buildIdStr = GetReadableBuildId(buildId, length);
            }
            if (!buildIdStr.empty()) {
                buildIds_.insert(std::pair<std::string, std::string>(std::string(mapPath), buildIdStr));
                frame->SetBuildId(buildIdStr);
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

    if (isValidFrame && (frame->GetFramePc() == frame->GetFrameRelativePc()) &&
        (frame->GetFrameMapName().find("Ark") == std::string::npos)) {
        isValidFrame = false;
    }

    bool ret = frame->GetFrameIndex() < MIN_VALID_FRAME_COUNT || isValidFrame;
    if (ret) {
        DfxRingBufferWrapper::GetInstance().AppendMsg(frame->ToString());
    }
    return ret;
}

bool DfxUnwindRemote::UnwindThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    if (!thread) {
        DFXLOG_ERROR("NULL thread needs unwind.");
        return false;
    }

    bool isCrash = thread->GetIsCrashThread() && (process->GetIsSignalDump() == false);
    pid_t nsTid = isCrash ? ProcessDumper::GetInstance().GetTargetNsPid() : thread->GetRealTid();
    pid_t tid = thread->GetThreadId();
    char buf[LOG_BUF_LEN] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Tid:%d, Name:%s\n", tid, thread->GetThreadName().c_str());
    if (ret <= 0) {
        DFXLOG_ERROR("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    DfxRingBufferWrapper::GetInstance().AppendMsg(std::string(buf));
    void *context = _UPT_create(nsTid);
    if (!context) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Failed to create unwind context for %d.\n", nsTid);
        return false;
    }

    if (!as_) {
        as_ = unw_create_addr_space(&_UPT_accessors, 0);
        if (!as_) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("Unwind address space is not exist for %d.\n", nsTid);
            _UPT_destroy(context);
            return false;
        }
        unw_set_caching_policy(as_, UNW_CACHE_GLOBAL);
    }

    unw_cursor_t cursor;
    int unwRet = unw_init_remote(&cursor, as_, context);
    if (unwRet != 0) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Failed to init cursor for thread:%d code:%d.\n", nsTid, unwRet);
        _UPT_destroy(context);
        return false;
    }

    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
    if (regs != nullptr) {
        std::vector<uintptr_t> regsVector = regs->GetRegsData();
        unw_set_context(&cursor, regsVector.data(), regsVector.size());
    }

    size_t index = 0;
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
    if (isCrash) {
        if (regs != nullptr) {
            DfxRingBufferWrapper::GetInstance().AppendMsg(regs->PrintRegs());
        }
        if (DfxConfig::GetInstance().GetDisplayFaultStack()) {
            thread->CreateFaultStack(nsTid);
            thread->CollectFaultMemorys(process->GetMaps());
            thread->PrintThreadFaultStackByConfig();
        }
    }
    _UPT_destroy(context);
    return true;
}

void DfxUnwindRemote::UnwindThreadFallback(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    // As we failed to init libunwind, just print pc and lr for first two frames
    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
    if (regs == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("RegisterInfo is not existed for crash process");
        return;
    }

    std::shared_ptr<DfxElfMaps> maps = process->GetMaps();
    if (maps == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("MapsInfo is not existed for crash process");
        return;
    }

    auto createFrame = [maps, thread] (int index, uintptr_t pc) {
        std::shared_ptr<DfxElfMap> map;
        auto frame = std::make_shared<DfxFrame>();
        frame->SetFramePc(pc);
        frame->SetFrameIndex(index);
        thread->AddFrame(frame);
        if (maps->FindMapByAddr(pc, map)) {
            frame->SetFrameMap(map);
            frame->CalculateRelativePc(map);
            frame->SetFrameMapName(map->GetMapPath());
        } else {
            frame->SetFrameRelativePc(pc);
            frame->SetFrameMapName(index == 0 ? "Not mapped pc" : "Not mapped lr");
        }
        DfxRingBufferWrapper::GetInstance().AppendMsg(frame->ToString());
    };

    createFrame(0, regs->GetPC());
    createFrame(1, regs->GetLR());
    DfxRingBufferWrapper::GetInstance().AppendMsg(regs->PrintRegs());
}
} // namespace HiviewDFX
} // namespace OHOS
