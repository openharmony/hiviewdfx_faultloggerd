/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include "dfx_symbols.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "libunwind.h"
#include "libunwind_i-ohos.h"
#include "process_dumper.h"
#include "printer.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
// we should have at least 2 frames, is pc and lr
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

DfxUnwindRemote::DfxUnwindRemote() : as_(nullptr)
{
    std::unique_ptr<DfxSymbols> symbols(new DfxSymbols());
    symbols_ = std::move(symbols);
}

bool DfxUnwindRemote::UnwindProcess(std::shared_ptr<ProcessDumpRequest> request, std::shared_ptr<DfxProcess> process)
{
    bool ret = false;
    if (process == nullptr) {
        DFXLOG_WARN("%s::can not unwind null process.", __func__);
        return ret;
    }

    as_ = unw_create_addr_space(&_UPT_accessors, 0);
    if (as_ == nullptr) {
        DFXLOG_WARN("%s::failed to create address space.", __func__);
        return ret;
    }
    unw_set_caching_policy(as_, UNW_CACHE_GLOBAL);

    std::shared_ptr<DfxThread> unwThread = process->keyThread_;
    if (ProcessDumper::GetInstance().IsCrash() && (process->vmThread_ != nullptr)) {
        unw_set_target_pid(as_, process->vmThread_->threadInfo_.tid);
        unwThread = process->vmThread_;
    } else {
        unw_set_target_pid(as_, process->processInfo_.pid);
    }

    do {
        if (unwThread == nullptr) {
            break;
        }

        ret = UnwindThread(process, unwThread);
        if (!ret) {
            UnwindThreadFallback(process, unwThread);
        }
        UnwindThreadByParseStackIfNeed(process, unwThread);
        Printer::PrintDumpHeader(request, process);
        Printer::PrintThreadHeaderByConfig(process->keyThread_);
        Printer::PrintThreadBacktraceByConfig(unwThread);
        if (ProcessDumper::GetInstance().IsCrash()) {
            Printer::PrintThreadRegsByConfig(unwThread);
            if (!DfxConfig::GetConfig().dumpOtherThreads) {
                break;
            }
        }

        auto threads = process->GetOtherThreads();
        if (threads.empty()) {
            break;
        }
        unw_set_target_pid(as_, process->processInfo_.pid);

        size_t index = 0;
        for (auto thread : threads) {
            if ((index == 0) && ProcessDumper::GetInstance().IsCrash()) {
                Printer::PrintOtherThreadHeaderByConfig();
            }

            if (thread->Attach()) {
                Printer::PrintThreadHeaderByConfig(thread);
                UnwindThread(process, thread);
                Printer::PrintThreadBacktraceByConfig(thread);
                thread->Detach();
            }
            index++;
        }
        ret = true;
    } while (false);

    if (ProcessDumper::GetInstance().IsCrash()) {
        Printer::PrintThreadFaultStackByConfig(process, unwThread);
        Printer::PrintProcessMapsByConfig(process);
    }

    unw_destroy_addr_space(as_);
    as_ = nullptr;
    return ret;
}

void DfxUnwindRemote::UnwindThreadByParseStackIfNeed(std::shared_ptr<DfxProcess> &process,
    std::shared_ptr<DfxThread> &thread)
{
    if (process == nullptr || thread == nullptr) {
        return;
    }
    auto frames = thread->GetFrames();
    constexpr int MIN_FRAMES_NUM = 3;
    if (frames.size() < MIN_FRAMES_NUM ||
        frames[MIN_FRAMES_NUM - 1]->mapName.find("Not mapped") != std::string::npos) {
        bool needParseStack = true;
        thread->InitFaultStack(needParseStack);
        std::vector<std::shared_ptr<DfxFrame>> newFrames;
        process->InitProcessMaps();
        auto faultStack = thread->GetFaultStack();
        if (faultStack == nullptr || !faultStack->ParseUnwindStack(process->GetMaps(), newFrames)) {
            DFXLOG_ERROR("%s : Failed to parse unwind stack.", __func__);
            return;
        }
        thread->SetFrames(newFrames);
        std::string tip = " Failed to unwind stack, try to get call stack by reparse thread stack";
        std::string msg = process->GetFatalMessage() + tip;
        process->SetFatalMessage(msg);
    }
}

uint64_t DfxUnwindRemote::DoAdjustPc(unw_cursor_t & cursor, uint64_t pc)
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

bool DfxUnwindRemote::DoUnwindStep(size_t const & index,
    std::shared_ptr<DfxThread>& thread, unw_cursor_t& cursor, std::shared_ptr<DfxProcess> process)
{
    bool ret = false;
    uint64_t framePc;
    static unw_word_t prevFrameSp = 0;
    if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&framePc))) {
        DFXLOG_WARN("Fail to get current pc.");
        return ret;
    }

    uint64_t frameSp;
    if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&frameSp))) {
        DFXLOG_WARN("Fail to get stack pointer.");
        return ret;
    }

    if (prevFrameSp == frameSp && index != 0) {
        return ret;
    }
    prevFrameSp = frameSp;

    uint64_t relPc = unw_get_rel_pc(&cursor);
    if (index != 0) {
        relPc = DoAdjustPc(cursor, relPc);
        framePc = DoAdjustPc(cursor, framePc);
#if defined(__arm__)
        unw_set_adjust_pc(&cursor, framePc);
#endif
    }

    if (relPc == 0) {
        relPc = framePc;
    }

    auto frame = std::make_shared<DfxFrame>();
    frame->relPc = relPc;
    frame->pc = framePc;
    frame->sp = frameSp;
    frame->index = index;
    ret = UpdateAndFillFrame(cursor, frame, process, thread, ProcessDumper::GetInstance().IsCrash());
    if (ret) {
        thread->AddFrame(frame);
    }

    DFXLOG_DEBUG("%s :: index(%d), framePc(0x%x), frameSp(0x%x).", __func__, index, framePc, frameSp);
    return ret;
}

bool DfxUnwindRemote::UpdateAndFillFrame(unw_cursor_t& cursor, std::shared_ptr<DfxFrame> frame,
    std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread, bool enableBuildId)
{
    struct map_info* mapInfo = unw_get_map(&cursor);
    bool isValidFrame = true;
    if (mapInfo != nullptr) {
        std::string mapPath = std::string(mapInfo->path);
        std::string funcName;
        bool isGetFuncName = false;
#if defined(__aarch64__)
        if ((frame->index == 0) && ((mapPath.find("ArkTS Code") != std::string::npos))) {
            isGetFuncName = GetArkJsHeapFuncName(funcName, thread);
            if (isGetFuncName) {
                frame->funcName = funcName;
            }
        }
#endif
        uint64_t funcOffset;
        if (!isGetFuncName && symbols_->GetNameAndOffsetByPc(as_, frame->pc, funcName, funcOffset)) {
            frame->funcName = funcName;
            frame->funcOffset = funcOffset;
        }

        if (mapPath.find(".hap") != std::string::npos) {
            char libraryName[PATH_LEN] = { 0 };
            if (unw_get_library_name_by_map(mapInfo, libraryName, PATH_LEN - 1) == 0) {
                mapPath = mapPath + "!" + std::string(libraryName);
            }
        }
        frame->mapName = mapPath;

        if (enableBuildId && buildIds_.find(mapPath) != buildIds_.end()) {
            frame->buildId = buildIds_[mapPath];
        } else if (enableBuildId && buildIds_.find(mapPath) == buildIds_.end()) {
            uint8_t* buildId = nullptr;
            size_t length = 0;
            std::string buildIdStr = "";
            if (unw_get_build_id(mapInfo, &buildId, &length)) {
                buildIdStr = GetReadableBuildId(buildId, length);
            }
            if (!buildIdStr.empty()) {
                buildIds_.insert(std::pair<std::string, std::string>(std::string(mapPath), buildIdStr));
                frame->buildId = buildIdStr;
            }
        }
    } else {
        process->InitProcessMaps();
        std::shared_ptr<DfxElfMaps> maps = process->GetMaps();
        std::shared_ptr<DfxElfMap> map;
        if (maps != nullptr && maps->FindMapByAddr(frame->pc, map)) {
            frame->relPc = map->GetRelPc(frame->pc);
            frame->mapName = map->path;
        } else {
            std::string tips = "Not mapped ";
            std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
            if (regs != nullptr) {
                tips.append(regs->GetSpecialRegisterName(frame->pc));
            }
            frame->mapName = tips;
            isValidFrame = false;
        }

    }

    if (isValidFrame && (frame->pc == frame->relPc) &&
        (frame->mapName.find("Ark") == std::string::npos)) {
        isValidFrame = false;
    }

    bool ret = frame->index < MIN_VALID_FRAME_COUNT || isValidFrame;
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
        std::vector<uintptr_t> regsData = regs->GetRegsData();
        if (regsData.size() < (UNW_AARCH64_X29 + 1)) {
            break;
        }
        uintptr_t x20 = regsData[UNW_AARCH64_X20];
        uintptr_t fp = regsData[UNW_AARCH64_X29];

        int result = unw_get_ark_js_heap_crash_info(thread->threadInfo_.tid,
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

bool DfxUnwindRemote::UnwindThread(std::shared_ptr<DfxProcess> process, std::shared_ptr<DfxThread> thread)
{
    if (thread == nullptr) {
        DFXLOG_ERROR("NULL thread needs unwind.");
        return false;
    }

    pid_t nsTid = thread->threadInfo_.nsTid;
    void *context = _UPT_create(nsTid);
    if (context == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Failed to create unwind context for %d.\n", nsTid);
        return false;
    }

    if (as_ == nullptr) {
        as_ = unw_create_addr_space(&_UPT_accessors, 0);
        if (as_ == nullptr) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("Unwind address space is not exist for %d.\n", nsTid);
            _UPT_destroy(context);
            return false;
        }
        unw_set_caching_policy(as_, UNW_CACHE_GLOBAL);
    }

    unw_cursor_t cursor;
    int unwRet = unw_init_remote(&cursor, as_, context);
    if (unwRet != 0) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Failed to init cursor for thread:%d ret:%d.\n", nsTid, unwRet);
        _UPT_destroy(context);
        return false;
    }

    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
    if (regs != nullptr) {
        std::vector<uintptr_t> regsData = regs->GetRegsData();
        unw_set_context(&cursor, regsData.data(), regsData.size());
    }

    size_t index = 0;
    do {
        // store current frame
        if (!DoUnwindStep(index, thread, cursor, process)) {
            break;
        }
        index++;

        // find next frame
        unwRet = unw_step(&cursor);
    } while ((unwRet > 0) && (index < DfxConfig::GetConfig().maxFrameNums));

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

    process->InitProcessMaps();
    std::shared_ptr<DfxElfMaps> maps = process->GetMaps();
    if (maps == nullptr) {
        DfxRingBufferWrapper::GetInstance().AppendMsg("MapsInfo is not existed for crash process");
        return;
    }

    auto createFrame = [maps, thread] (size_t index, uintptr_t pc) {
        std::shared_ptr<DfxElfMap> map;
        auto frame = std::make_shared<DfxFrame>();
        frame->pc = pc;
        frame->index = index;
        if (maps->FindMapByAddr(pc, map)) {
            frame->relPc = map->GetRelPc(pc);
            frame->mapName = map->path;
        } else {
            frame->relPc = pc;
            frame->mapName = (index == 0 ? "Not mapped pc" : "Not mapped lr");
        }
        thread->AddFrame(frame);
    };

    createFrame(0, regs->pc_);
    createFrame(1, regs->lr_);
}
} // namespace HiviewDFX
} // namespace OHOS
