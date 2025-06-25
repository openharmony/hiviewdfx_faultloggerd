/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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

#include "unwinder.h"

#include <unordered_map>
#include <dlfcn.h>
#include <link.h>

#include "dfx_ark.h"
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_frame_formatter.h"
#include "dfx_hap.h"
#include "dfx_instructions.h"
#include "dfx_jsvm.h"
#include "dfx_log.h"
#include "dfx_memory.h"
#include "dfx_param.h"
#include "dfx_regs_get.h"
#include "dfx_symbols.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "elapsed_time.h"
#include "fp_unwinder.h"
#include "stack_utils.h"
#include "string_printf.h"
#include "string_util.h"
#include "thread_context.h"
#include "unwind_entry_parser_factory.h"

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwinder"

class Unwinder::Impl {
public:
    // for local
    Impl(bool needMaps) : pid_(UNWIND_TYPE_LOCAL)
    {
        if (needMaps) {
            maps_ = DfxMaps::Create();
        }
        enableFpCheckMapExec_ = true;
        memory_ = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
        Init();
    }

    // for remote
    Impl(int pid, bool crash) : pid_(pid)
    {
        if (pid <= 0) {
            DFXLOGE("pid(%{public}d) error", pid);
            return;
        }
        DfxEnableTraceDlsym(true);
        maps_ = DfxMaps::Create(pid, crash);
        enableFpCheckMapExec_ = true;
        memory_ = std::make_shared<DfxMemory>(UNWIND_TYPE_REMOTE);
        Init();
    }

    Impl(int pid, int nspid, bool crash) : pid_(nspid)
    {
        if (pid <= 0 || nspid <= 0) {
            DFXLOGE("pid(%{public}d) or nspid(%{public}d) error", pid, nspid);
            return;
        }
        DfxEnableTraceDlsym(true);
        maps_ = DfxMaps::Create(pid, crash);
        enableFpCheckMapExec_ = true;
        isCrash_ = crash;
        memory_ = std::make_shared<DfxMemory>(UNWIND_TYPE_REMOTE);
        Init();
    }

    // for customized
    Impl(const std::shared_ptr<UnwindAccessors> &accessors, bool local)
    {
        UnwindType unwindType = local ? UNWIND_TYPE_CUSTOMIZE_LOCAL : UNWIND_TYPE_CUSTOMIZE;
        pid_ = unwindType;
        enableFpCheckMapExec_ = false;
        enableFillFrames_ = false;
    #if defined(__aarch64__)
        pacMask_ = pacMaskDefault_;
    #endif
        memory_ = std::make_shared<DfxMemory>(unwindType, accessors);
        Init();
    }

    ~Impl()
    {
        DfxEnableTraceDlsym(false);
        Destroy();
#if defined(ENABLE_MIXSTACK)
        if (isArkCreateLocal_) {
            if (DfxArk::Instance().ArkDestroyLocal() < 0) {
                DFXLOGU("Failed to ark destroy local.");
            }
        }
#endif
        if (pid_ == UNWIND_TYPE_LOCAL) {
            LocalThreadContext::GetInstance().CleanUp();
        }
    }

    inline void EnableUnwindCache(bool enableCache)
    {
        enableCache_ = enableCache;
    }
    inline void EnableFpCheckMapExec(bool enableFpCheckMapExec)
    {
        enableFpCheckMapExec_ = enableFpCheckMapExec;
    }
    inline void EnableFillFrames(bool enableFillFrames)
    {
        enableFillFrames_ = enableFillFrames;
    }
    inline void EnableParseNativeSymbol(bool enableParseNativeSymbol)
    {
        enableParseNativeSymbol_ = enableParseNativeSymbol;
    }
    inline void EnableJsvmstack(bool enableJsvmstack)
    {
        enableJsvmstack_ = enableJsvmstack;
    }
    inline void IgnoreMixstack(bool ignoreMixstack)
    {
        ignoreMixstack_ = ignoreMixstack;
    }

    inline void SetRegs(const std::shared_ptr<DfxRegs> &regs)
    {
        if (regs == nullptr) {
            return;
        }
        regs_ = regs;
        firstFrameSp_ = regs_->GetSp();
    }

    inline const std::shared_ptr<DfxRegs>& GetRegs() const
    {
        return regs_;
    }

    inline const std::shared_ptr<DfxMaps>& GetMaps() const
    {
        return maps_;
    }

    inline uint16_t GetLastErrorCode()
    {
        return lastErrorData_.GetCode();
    }
    inline uint64_t GetLastErrorAddr()
    {
        return lastErrorData_.GetAddr();
    }

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);

    bool UnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindLocalWithTid(pid_t tid, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindLocalByOtherTid(pid_t tid, bool fast, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum, bool enableArk = false);
    bool UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum);
    bool Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk);

    bool Step(uintptr_t& pc, uintptr_t& sp, void *ctx);
    bool FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx);

    void AddFrame(DfxFrame& frame);
    const std::vector<DfxFrame>& GetFrames();
    inline const std::vector<uintptr_t>& GetPcs() const
    {
        return pcs_;
    }
    void FillFrames(std::vector<DfxFrame>& frames);
    void FillFrame(DfxFrame& frame, bool needSymParse = true);
    void ParseFrameSymbol(DfxFrame& frame);
    void FillJsFrame(DfxFrame& frame);
    bool GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame);
    void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs);
    inline void SetIsJitCrashFlag(bool isCrash)
    {
        isJitCrash_ = isCrash;
    }
    int ArkWriteJitCodeToFile(int fd);
    bool GetLockInfo(int32_t tid, char* buf, size_t sz);
    void SetFrames(const std::vector<DfxFrame>& frames)
    {
        frames_ = frames;
    }
    inline const std::vector<uintptr_t>& GetJitCache(void) const
    {
        return jitCache_;
    }
    static int DlPhdrCallback(struct dl_phdr_info *info, size_t size, void *data);
private:
    bool FillJsFrameLocal(DfxFrame& frame, JsFunction* jsFunction);

private:
    struct StepFrame {
        uintptr_t pc = 0;
        uintptr_t sp = 0;
        uintptr_t fp = 0;
        bool isJsFrame {false};
        bool isJsvmFrame {false};
    };
    struct StepCache {
        std::shared_ptr<DfxMap> map = nullptr;
        std::shared_ptr<RegLocState> rs = nullptr;
    };
    void Init();
    void Clear();
    void Destroy();
    void InitParam()
    {
#if defined(ENABLE_MIXSTACK)
        enableMixstack_ = DfxParam::IsEnableMixstack();
#endif
    }
    bool GetMainStackRangeInner(uintptr_t& stackBottom, uintptr_t& stackTop);
    bool GetArkStackRangeInner(uintptr_t& arkMapStart, uintptr_t& arkMapEnd);
    bool CheckAndReset(void* ctx);
    void DoPcAdjust(uintptr_t& pc);
    void AddFrame(const StepFrame& frame, std::shared_ptr<DfxMap> map);
    bool FindCache(uintptr_t pc, std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs);
    bool AddFrameMap(const StepFrame& frame, std::shared_ptr<DfxMap>& map);
    bool UnwindArkFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map, bool& stopUnwind);
    bool ParseUnwindTable(uintptr_t pc, std::shared_ptr<RegLocState>& rs);
    void UpdateRegsState(StepFrame& frame, void* ctx, bool& unwinderResult, std::shared_ptr<RegLocState>& rs);
    bool CheckFrameValid(const StepFrame& frame, const std::shared_ptr<DfxMap>& map, uintptr_t prevSp);
    bool StepInner(const bool isSigFrame, StepFrame& frame, void *ctx);
    bool Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);
    bool UnwindFrame(void *ctx, StepFrame& frame, bool& needAdjustPc);
#if defined(ENABLE_MIXSTACK)
    bool StepArkJsFrame(StepFrame& frame);
#endif
    bool StepJsvmFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map);
    inline void SetLocalStackCheck(void* ctx, bool check) const
    {
        if ((pid_ == UNWIND_TYPE_LOCAL) && (ctx != nullptr)) {
            UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
            uctx->stackCheck = check;
        }
    }

#if defined(__aarch64__)
    MAYBE_UNUSED const uintptr_t pacMaskDefault_ = static_cast<uintptr_t>(0xFFFFFF8000000000);
#endif
    bool enableCache_ = true;
    bool enableFillFrames_ = true;
    bool enableParseNativeSymbol_ = true;
    bool enableLrFallback_ = true;
    bool enableFpCheckMapExec_ = false;
    bool isCrash_ = false;
    bool isFpStep_ = false;
    bool isArkCreateLocal_ = false;
    bool isResetFrames_ = false;
    bool enableJsvmstack_ = false;
    MAYBE_UNUSED bool enableMixstack_ = true;
    MAYBE_UNUSED bool ignoreMixstack_ = false;
    MAYBE_UNUSED bool stopWhenArkFrame_ = false;
    MAYBE_UNUSED bool isJitCrash_ = false;

    int32_t pid_ = 0;
    uintptr_t pacMask_ = 0;
    std::vector<uintptr_t> jitCache_ = {};
    std::shared_ptr<DfxMemory> memory_ = nullptr;
    std::unordered_map<uintptr_t, StepCache> stepCache_ {};
    std::shared_ptr<DfxRegs> regs_ = nullptr;
    std::shared_ptr<DfxMaps> maps_ = nullptr;
    std::vector<uintptr_t> pcs_ {};
    std::vector<DfxFrame> frames_ {};
    UnwindErrorData lastErrorData_ {};
    std::shared_ptr<UnwindEntryParser> unwindEntryParser_ = nullptr;
    uintptr_t firstFrameSp_ {0};
};

// for local
Unwinder::Unwinder(bool needMaps) : impl_(std::make_shared<Impl>(needMaps))
{
}

// for remote
Unwinder::Unwinder(int pid, bool crash) : impl_(std::make_shared<Impl>(pid, crash))
{
}

Unwinder::Unwinder(int pid, int nspid, bool crash) : impl_(std::make_shared<Impl>(pid, nspid, crash))
{
}

// for customized
Unwinder::Unwinder(std::shared_ptr<UnwindAccessors> accessors, bool local)
    : impl_(std::make_shared<Impl>(accessors, local))
{
}

void Unwinder::EnableUnwindCache(bool enableCache)
{
    impl_->EnableUnwindCache(enableCache);
}

void Unwinder::EnableFpCheckMapExec(bool enableFpCheckMapExec)
{
    impl_->EnableFpCheckMapExec(enableFpCheckMapExec);
}

void Unwinder::EnableFillFrames(bool enableFillFrames)
{
    impl_->EnableFillFrames(enableFillFrames);
}

void Unwinder::EnableParseNativeSymbol(bool enableParseNativeSymbol)
{
    impl_->EnableParseNativeSymbol(enableParseNativeSymbol);
}

void Unwinder::EnableJsvmstack(bool enableJsvmstack)
{
    impl_->EnableJsvmstack(enableJsvmstack);
}

void Unwinder::IgnoreMixstack(bool ignoreMixstack)
{
    impl_->IgnoreMixstack(ignoreMixstack);
}

void Unwinder::SetRegs(std::shared_ptr<DfxRegs> regs)
{
    impl_->SetRegs(regs);
}

const std::shared_ptr<DfxRegs>& Unwinder::GetRegs() const
{
    return impl_->GetRegs();
}

const std::shared_ptr<DfxMaps>& Unwinder::GetMaps() const
{
    return impl_->GetMaps();
}

uint16_t Unwinder::GetLastErrorCode() const
{
    return impl_->GetLastErrorCode();
}

uint64_t Unwinder::GetLastErrorAddr() const
{
    return impl_->GetLastErrorAddr();
}

bool Unwinder::GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    return impl_->GetStackRange(stackBottom, stackTop);
}

bool Unwinder::UnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindLocalWithContext(context, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindLocalWithTid(pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindLocalWithTid(tid, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindLocalByOtherTid(pid_t tid, bool fast, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindLocalByOtherTid(tid, fast, maxFrameNum, skipFrameNum);
}

bool __attribute__((optnone)) Unwinder::UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum,
    size_t skipFrameNum, bool enableArk)
{
    return impl_->UnwindLocal(withRegs, fpUnwind, maxFrameNum, skipFrameNum + 1, enableArk);
}

bool Unwinder::UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindRemote(tid, withRegs, maxFrameNum, skipFrameNum);
}

bool Unwinder::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->Unwind(ctx, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
{
    return impl_->UnwindByFp(ctx, maxFrameNum, skipFrameNum, enableArk);
}

bool Unwinder::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    return impl_->Step(pc, sp, ctx);
}

bool Unwinder::FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx)
{
    return impl_->FpStep(fp, pc, ctx);
}

void Unwinder::AddFrame(DfxFrame& frame)
{
    impl_->AddFrame(frame);
}

const std::vector<DfxFrame>& Unwinder::GetFrames() const
{
    return impl_->GetFrames();
}

const std::vector<uintptr_t>& Unwinder::GetPcs() const
{
    return impl_->GetPcs();
}

void Unwinder::FillFrames(std::vector<DfxFrame>& frames)
{
    impl_->FillFrames(frames);
}

void Unwinder::FillFrame(DfxFrame& frame, bool needSymParse)
{
    impl_->FillFrame(frame, needSymParse);
}

void Unwinder::ParseFrameSymbol(DfxFrame& frame)
{
    impl_->ParseFrameSymbol(frame);
}

void Unwinder::FillJsFrame(DfxFrame& frame)
{
    impl_->FillJsFrame(frame);
}

bool Unwinder::GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame)
{
    return impl_->GetFrameByPc(pc, maps, frame);
}

void Unwinder::GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    impl_->GetFramesByPcs(frames, pcs);
}

void Unwinder::SetIsJitCrashFlag(bool isCrash)
{
    impl_->SetIsJitCrashFlag(isCrash);
}

int Unwinder::ArkWriteJitCodeToFile(int fd)
{
    return impl_->ArkWriteJitCodeToFile(fd);
}

const std::vector<uintptr_t>& Unwinder::GetJitCache()
{
    return impl_->GetJitCache();
}

bool Unwinder::GetLockInfo(int32_t tid, char* buf, size_t sz)
{
    return impl_->GetLockInfo(tid, buf, sz);
}

void Unwinder::SetFrames(std::vector<DfxFrame>& frames)
{
    impl_->SetFrames(frames);
}

void Unwinder::Impl::Init()
{
    Destroy();
    unwindEntryParser_ = UnwindEntryParserFactory::CreateUnwindEntryParser(memory_);
    InitParam();
#if defined(ENABLE_MIXSTACK)
    DFXLOGD("Unwinder mixstack enable: %{public}d", enableMixstack_);
#else
    DFXLOGD("Unwinder init");
#endif
}

void Unwinder::Impl::Clear()
{
    isFpStep_ = false;
    enableMixstack_ = true;
    pcs_.clear();
    frames_.clear();
    lastErrorData_ = {};
}

void Unwinder::Impl::Destroy()
{
    Clear();
    stepCache_.clear();
}

bool Unwinder::Impl::CheckAndReset(void* ctx)
{
    if ((ctx == nullptr) || (memory_ == nullptr)) {
        return false;
    }
    memory_->SetCtx(ctx);
    return true;
}

bool Unwinder::Impl::GetMainStackRangeInner(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (maps_ != nullptr && !maps_->GetStackRange(stackBottom, stackTop)) {
        return false;
    } else if (maps_ == nullptr && !StackUtils::Instance().GetMainStackRange(stackBottom, stackTop)) {
        return false;
    }
    return true;
}

bool Unwinder::Impl::GetArkStackRangeInner(uintptr_t& arkMapStart, uintptr_t& arkMapEnd)
{
    if (maps_ != nullptr && !maps_->GetArkStackRange(arkMapStart, arkMapEnd)) {
        return false;
    } else if (maps_ == nullptr && !StackUtils::Instance().GetArkStackRange(arkMapStart, arkMapEnd)) {
        return false;
    }
    return true;
}

bool Unwinder::Impl::GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (gettid() == getpid()) {
        return GetMainStackRangeInner(stackBottom, stackTop);
    }
    return StackUtils::GetSelfStackRange(stackBottom, stackTop);
}

bool Unwinder::Impl::UnwindLocalWithTid(const pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
{
    if (tid < 0 || tid == gettid()) {
        lastErrorData_.SetCode(UNW_ERROR_NOT_SUPPORT);
        DFXLOGE("params is nullptr, tid: %{public}d", tid);
        return false;
    }
    DFXLOGD("UnwindLocalWithTid:: tid: %{public}d", tid);
    auto threadContext = LocalThreadContext::GetInstance().CollectThreadContext(tid);
#if defined(__aarch64__)
    if (threadContext != nullptr && threadContext->frameSz > 0) {
        pcs_.clear();
        for (size_t i = 0; i < threadContext->frameSz; i++) {
            pcs_.emplace_back(threadContext->pcs[i]);
        }
        firstFrameSp_ = threadContext->firstFrameSp;
        return true;
    }
    return false;
#else
    if (threadContext == nullptr || threadContext->ctx == nullptr) {
        DFXLOGW("Failed to get thread context of tid(%{public}d)", tid);
        LocalThreadContext::GetInstance().ReleaseThread(tid);
        return false;
    }
    if (regs_ == nullptr) {
        regs_ = DfxRegs::CreateFromUcontext(*(threadContext->ctx));
    } else {
        regs_->SetFromUcontext(*(threadContext->ctx));
    }
    uintptr_t stackBottom = 1;
    uintptr_t stackTop = static_cast<uintptr_t>(-1);
    if (tid == getpid()) {
        if (!GetMainStackRangeInner(stackBottom, stackTop)) {
            return false;
        }
    } else if (!LocalThreadContext::GetInstance().GetStackRange(tid, stackBottom, stackTop)) {
        DFXLOGE("Failed to get stack range with tid(%{public}d), err(%{public}d)", tid, errno);
        return false;
    }
    if (stackBottom == 0 || stackTop == 0) {
        DFXLOGE("Invalid stack range, err(%{public}d)", errno);
        return false;
    }
    DFXLOGU("[%{public}d]: stackBottom: %{public}" PRIx64 ", stackTop: %{public}" PRIx64 "", __LINE__,
        (uint64_t)stackBottom, (uint64_t)stackTop);
    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs_;
    context.maps = maps_;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    auto ret = Unwind(&context, maxFrameNum, skipFrameNum);
    LocalThreadContext::GetInstance().ReleaseThread(tid);
    return ret;
#endif
}

bool Unwinder::Impl::UnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        regs_ = DfxRegs::CreateFromUcontext(context);
    } else {
        regs_->SetFromUcontext(context);
    }
    return UnwindLocal(true, false, maxFrameNum, skipFrameNum);
}

bool Unwinder::Impl::UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
{
    DFXLOGI("UnwindLocal:: fpUnwind: %{public}d", fpUnwind);
    uintptr_t stackBottom = 1;
    uintptr_t stackTop = static_cast<uintptr_t>(-1);
    if (!GetStackRange(stackBottom, stackTop)) {
        DFXLOGE("Get stack range error");
        return false;
    }

    if (!withRegs) {
#if defined(__aarch64__)
        if (fpUnwind) {
            uintptr_t miniRegs[FP_MINI_REGS_SIZE] = {0};
            GetFramePointerMiniRegs(miniRegs, sizeof(miniRegs) / sizeof(miniRegs[0]));
            regs_ = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, miniRegs,
                                            sizeof(miniRegs) / sizeof(miniRegs[0]));
            withRegs = true;
        }
#endif
        if (!withRegs) {
            regs_ = DfxRegs::Create();
            auto regsData = regs_->RawData();
            if (regsData == nullptr) {
                DFXLOGE("[%{public}d]: params is nullptr", __LINE__);
                return false;
            }
            GetLocalRegs(regsData);
        }
    }

    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs_;
    context.maps = maps_;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
#ifdef __aarch64__
    if (fpUnwind) {
        return UnwindByFp(&context, maxFrameNum, skipFrameNum, enableArk);
    }
#endif
    return Unwind(&context, maxFrameNum, skipFrameNum);
}

bool Unwinder::Impl::UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    std::string timeLimitCheck =
        "Unwinder::Impl::UnwindRemote, tid: " + std::to_string(tid);
    ElapsedTime counter(std::move(timeLimitCheck), 20); // 20 : limit cost time 20 ms
    if ((maps_ == nullptr) || (pid_ <= 0) || (tid < 0)) {
        DFXLOGE("params is nullptr, pid: %{public}d, tid: %{public}d", pid_, tid);
        return false;
    }
    if (tid == 0) {
        tid = pid_;
    }
    if (!withRegs) {
        regs_ = DfxRegs::CreateRemoteRegs(tid);
    }
    if ((regs_ == nullptr)) {
        DFXLOGE("regs is nullptr");
        return false;
    }

    firstFrameSp_ = regs_->GetSp();
    UnwindContext context;
    context.pid = tid;
    context.regs = regs_;
    context.maps = maps_;
    return Unwind(&context, maxFrameNum, skipFrameNum);
}

int Unwinder::Impl::ArkWriteJitCodeToFile(int fd)
{
#if defined(ENABLE_MIXSTACK)
    return DfxArk::Instance().JitCodeWriteFile(memory_.get(), &(Unwinder::AccessMem), fd,
        jitCache_.data(), jitCache_.size());
#else
    return -1;
#endif
}

#if defined(ENABLE_MIXSTACK)
bool Unwinder::Impl::StepArkJsFrame(StepFrame& frame)
{
    DFX_TRACE_SCOPED_DLSYM("StepArkJsFrame pc: %p", reinterpret_cast<void *>(frame.pc));
    std::string timeLimitCheck;
    timeLimitCheck += "StepArkJsFrame, ark pc: " + std::to_string(frame.pc) +
        ", fp:" + std::to_string(frame.fp) + ", sp:" + std::to_string(frame.sp) +
        ", isJsFrame:" + std::to_string(frame.isJsFrame);
    ElapsedTime counter(timeLimitCheck, 20); // 20 : limit cost time 20 ms
    if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
        DFXLOGD("+++ark pc: %{private}p, fp: %{private}p, sp: %{private}p, isJsFrame: %{public}d.",
            reinterpret_cast<void *>(frame.pc),
            reinterpret_cast<void *>(frame.fp), reinterpret_cast<void *>(frame.sp), frame.isJsFrame);
    }

    int ret = -1;
    if (isJitCrash_) {
        MAYBE_UNUSED uintptr_t methodId = 0;
        ArkUnwindParam arkParam(memory_.get(), &(Unwinder::AccessMem), &frame.fp, &frame.sp, &frame.pc,
            &methodId, &frame.isJsFrame, jitCache_);
        ret = DfxArk::Instance().StepArkFrameWithJit(&arkParam);
    } else {
        ArkStepParam arkParam(&frame.fp, &frame.sp, &frame.pc, &frame.isJsFrame);
        ret = DfxArk::Instance().StepArkFrame(memory_.get(), &(Unwinder::AccessMem), &arkParam);
    }
    if (ret < 0) {
        DFXLOGE("Failed to step ark frame");
        return false;
    }
    if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
        DFXLOGD("---ark pc: %{private}p, fp: %{private}p, sp: %{private}p, isJsFrame: %{public}d.",
            reinterpret_cast<void *>(frame.pc),
            reinterpret_cast<void *>(frame.fp), reinterpret_cast<void *>(frame.sp), frame.isJsFrame);
    }
    regs_->SetPc(StripPac(frame.pc, pacMask_));
    regs_->SetSp(frame.sp);
    regs_->SetFp(frame.fp);
    return true;
}
#endif

bool Unwinder::Impl::StepJsvmFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map)
{
    if (enableJsvmstack_ && ((map != nullptr && map->IsJsvmExecutable()) || frame.isJsvmFrame)) {
        DFX_TRACE_SCOPED_DLSYM("StepJsvmFrame pc: %p", reinterpret_cast<void *>(frame.pc));
        std::string timeLimitCheck;
        timeLimitCheck += "StepJsvmFrame, pc: " + std::to_string(frame.pc) +
            ", fp:" + std::to_string(frame.fp) + ", sp:" + std::to_string(frame.sp) +
            ", isJsvmFrame:" + std::to_string(frame.isJsvmFrame);
        ElapsedTime counter(timeLimitCheck, 20); // 20 : limit cost time 20 ms
        JsvmStepParam jsvmParam(&frame.fp, &frame.sp, &frame.pc, &frame.isJsvmFrame);
        if (DfxJsvm::Instance().StepJsvmFrame(memory_.get(), &(Unwinder::AccessMem), &jsvmParam) < 0) {
            DFXLOGE("Failed to step ark frame");
            return false;
        }
        regs_->SetPc(StripPac(frame.pc, pacMask_));
        regs_->SetSp(frame.sp);
        regs_->SetFp(frame.fp);
    }
    return true;
}

bool Unwinder::Impl::UnwindFrame(void *ctx, StepFrame& frame, bool& needAdjustPc)
{
    frame.pc = regs_->GetPc();
    frame.sp = regs_->GetSp();
    frame.fp = regs_->GetFp();
    // Check if this is a signal frame.
    if (pid_ != UNWIND_TYPE_LOCAL && pid_ != UNWIND_TYPE_CUSTOMIZE_LOCAL &&
        regs_->StepIfSignalFrame(static_cast<uintptr_t>(frame.pc), memory_)) {
        DFXLOGW("Step signal frame, pc: %{private}p", reinterpret_cast<void *>(frame.pc));
        StepInner(true, frame, ctx);
        return true;
    }
    if (!frame.isJsFrame && needAdjustPc) {
        DoPcAdjust(frame.pc);
    }
    needAdjustPc = true;
    uintptr_t prevPc = frame.pc;
    uintptr_t prevSp = frame.sp;
    if (!StepInner(false, frame, ctx)) {
        return false;
    }
    if (frame.pc == prevPc && frame.sp == prevSp) {
        if (pid_ >= 0) {
            MAYBE_UNUSED UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
            DFXLOGU("pc and sp is same, tid: %{public}d", uctx->pid);
        } else {
            DFXLOGU("pc and sp is same");
        }
        lastErrorData_.SetAddrAndCode(frame.pc, UNW_ERROR_REPEATED_FRAME);
        return false;
    }
    return true;
}

bool Unwinder::Impl::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        DFXLOGE("[%{public}d]: params is nullptr?", __LINE__);
        lastErrorData_.SetCode(UNW_ERROR_INVALID_CONTEXT);
        return false;
    }
    SetLocalStackCheck(ctx, false);
    Clear();

    bool needAdjustPc = false;
    isResetFrames_ = false;
    StepFrame frame;
    do {
        if (!isResetFrames_ && (skipFrameNum != 0) && (frames_.size() >= skipFrameNum)) {
            isResetFrames_ = true;
            DFXLOGU("frames size: %{public}zu, will be reset frames", frames_.size());
            frames_.clear();
        }
        if (frames_.size() >= maxFrameNum) {
            DFXLOGW("frames size: %{public}zu", frames_.size());
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        if (!UnwindFrame(ctx, frame, needAdjustPc)) {
            break;
        }
    } while (true);
    DFXLOGU("Last error code: %{public}d, addr: %{public}p",
        (int)GetLastErrorCode(), reinterpret_cast<void *>(GetLastErrorAddr()));
    DFXLOGU("Last frame size: %{public}zu, last frame pc: %{public}p",
        frames_.size(), reinterpret_cast<void *>(regs_->GetPc()));
    return (frames_.size() > 0);
}

bool Unwinder::Impl::UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
{
    if (regs_ == nullptr) {
        DFXLOGE("[%{public}d]: params is nullptr?", __LINE__);
        return false;
    }
    pcs_.clear();
    bool needAdjustPc = false;
    bool resetFrames = false;
    MAYBE_UNUSED bool isGetArkRange = false;
    MAYBE_UNUSED uintptr_t arkMapStart = 0;
    MAYBE_UNUSED uintptr_t arkMapEnd = 0;
    if (enableArk) {
        isGetArkRange = GetArkStackRangeInner(arkMapStart, arkMapEnd);
    }

    StepFrame frame;
    do {
        if (!resetFrames && skipFrameNum != 0 && (pcs_.size() == skipFrameNum)) {
            DFXLOGU("pcs size: %{public}zu, will be reset pcs", pcs_.size());
            resetFrames = true;
            pcs_.clear();
        }
        if (pcs_.size() >= maxFrameNum) {
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        frame.pc = regs_->GetPc();
        frame.sp = regs_->GetSp();
        frame.fp = regs_->GetFp();
        if (!frame.isJsFrame && needAdjustPc) {
            DoPcAdjust(frame.pc);
        }
        needAdjustPc = true;
        pcs_.emplace_back(frame.pc);
#if defined(ENABLE_MIXSTACK)
        if (enableArk && isGetArkRange && ((frame.pc >= arkMapStart && frame.pc < arkMapEnd) || frame.isJsFrame)) {
            if (StepArkJsFrame(frame)) {
                continue;
            }
            break;
        }
#endif

        if (!FpStep(frame.fp, frame.pc, ctx) || (frame.pc == 0)) {
            break;
        }
    } while (true);
    return (pcs_.size() > 0);
}

bool Unwinder::Impl::FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx)
{
#if defined(__aarch64__)
    DFXLOGU("+fp: %{public}lx, pc: %{public}lx", (uint64_t)fp, (uint64_t)pc);
    if ((regs_ == nullptr) || (memory_ == nullptr)) {
        DFXLOGE("[%{public}d]: params is nullptr", __LINE__);
        return false;
    }
    if (ctx != nullptr) {
        memory_->SetCtx(ctx);
    }

    uintptr_t prevFp = fp;
    uintptr_t ptr = fp;
    if (ptr != 0 && memory_->Read<uintptr_t>(ptr, &fp, true) &&
        memory_->Read<uintptr_t>(ptr, &pc, false)) {
        if (fp != 0 && fp <= prevFp) {
            DFXLOGU("Illegal or same fp value");
            lastErrorData_.SetAddrAndCode(pc, UNW_ERROR_ILLEGAL_VALUE);
            return false;
        }
        regs_->SetReg(REG_FP, &fp);
        regs_->SetReg(REG_SP, &prevFp);
        regs_->SetPc(StripPac(pc, pacMask_));
        DFXLOGU("-fp: %{public}lx, pc: %{public}lx", (uint64_t)fp, (uint64_t)pc);
        return true;
    }
#endif
    return false;
}

bool Unwinder::Impl::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    DFX_TRACE_SCOPED_DLSYM("Step pc:%p", reinterpret_cast<void *>(pc));
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        DFXLOGE("[%{public}d]: params is nullptr?", __LINE__);
        return false;
    }
    bool ret = false;
    StepFrame frame;
    frame.pc = pc;
    frame.sp = sp;
    frame.fp = regs_->GetFp();
    // Check if this is a signal frame.
    if (regs_->StepIfSignalFrame(frame.pc, memory_)) {
        DFXLOGW("Step signal frame, pc: %{private}p", reinterpret_cast<void *>(frame.pc));
        ret = StepInner(true, frame, ctx);
    } else {
        ret = StepInner(false, frame, ctx);
    }
    pc = frame.pc;
    sp = frame.sp;
    return ret;
}

bool Unwinder::Impl::FindCache(uintptr_t pc, std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs)
{
    if (enableCache_ && !isFpStep_) {
        auto iter = stepCache_.find(pc);
        if (iter != stepCache_.end()) {
            if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                DFXLOGU("Find rs cache, pc: %{public}p", reinterpret_cast<void *>(pc));
            }
            rs = iter->second.rs;
            map = iter->second.map;
            return true;
        }
    }
    return false;
}

bool Unwinder::Impl::AddFrameMap(const StepFrame& frame, std::shared_ptr<DfxMap>& map)
{
    int mapRet = memory_->GetMapByPc(frame.pc, map);
    if (mapRet != UNW_ERROR_NONE) {
        if (frame.isJsFrame) {
            DFXLOGW("Failed to get map with ark, frames size: %{public}zu", frames_.size());
            mapRet = UNW_ERROR_UNKNOWN_ARK_MAP;
        }
        if (frames_.size() > 2) { // 2, least 2 frame
            DFXLOGU("Failed to get map, frames size: %{public}zu", frames_.size());
            lastErrorData_.SetAddrAndCode(frame.pc, mapRet);
            return false;
        }
    }
    AddFrame(frame, map);
    return true;
}

bool Unwinder::Impl::UnwindArkFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map, bool& stopUnwind)
{
#if defined(ENABLE_MIXSTACK)
    if (stopWhenArkFrame_ && (map != nullptr && map->IsArkExecutable())) {
        DFXLOGU("Stop by ark frame");
        stopUnwind = true;
        return false;
    }
    if ((enableMixstack_) && ((map != nullptr && map->IsArkExecutable()) || frame.isJsFrame)) {
        if (!StepArkJsFrame(frame)) {
            DFXLOGE("Failed to step ark Js frame, pc: %{private}p", reinterpret_cast<void *>(frame.pc));
            lastErrorData_.SetAddrAndCode(frame.pc, UNW_ERROR_STEP_ARK_FRAME);
            return false;
        }
        stopUnwind = true;
        return true;
    }
#endif
    return true;
}

bool Unwinder::Impl::ParseUnwindTable(uintptr_t pc, std::shared_ptr<RegLocState>& rs)
{
    // find unwind table and entry
    UnwindTableInfo uti;
    int utiRet = memory_->FindUnwindTable(pc, uti);
    if (utiRet != UNW_ERROR_NONE) {
        lastErrorData_.SetAddrAndCode(pc, utiRet);
        DFXLOGU("Failed to find unwind table ret: %{public}d", utiRet);
        return false;
    }
    rs = std::make_shared<RegLocState>();
    // parse instructions and get cache rs
    if (!unwindEntryParser_->Step(pc, uti, rs)) {
        lastErrorData_.SetAddrAndCode(unwindEntryParser_->GetLastErrorAddr(), unwindEntryParser_->GetLastErrorCode());
        DFXLOGU("Step unwind entry failed");
        return false;
    }
    return true;
}

void Unwinder::Impl::UpdateRegsState(
    StepFrame& frame, void* ctx, bool& unwinderResult, std::shared_ptr<RegLocState>& rs)
{
    SetLocalStackCheck(ctx, true);
    if (unwinderResult) {
#if defined(__arm__) || defined(__aarch64__)
    auto lr = *(regs_->GetReg(REG_LR));
#endif
    unwinderResult = Apply(regs_, rs);
#if defined(__arm__) || defined(__aarch64__)
    if (!unwinderResult && enableLrFallback_ && (frames_.size() == 1)) {
        regs_->SetPc(lr);
        unwinderResult = true;
        if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
            DFXLOGW("Failed to apply first frame, lr fallback");
        }
    }
#endif
    } else {
        if (enableLrFallback_ && (frames_.size() == 1 && !isResetFrames_) && regs_->SetPcFromReturnAddress(memory_)) {
            unwinderResult = true;
            if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                DFXLOGW("Failed to step first frame, lr fallback");
            }
        }
    }
    regs_->SetPc(StripPac(regs_->GetPc(), pacMask_));

#if defined(__aarch64__)
    if (!unwinderResult) { // try fp
        unwinderResult = FpStep(frame.fp, frame.pc, ctx);
        if (unwinderResult && !isFpStep_) {
            if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                DFXLOGI("First enter fp step, pc: %{private}p", reinterpret_cast<void *>(frame.pc));
            }
            isFpStep_ = true;
        }
    }
#endif
    frame.pc = regs_->GetPc();
    frame.sp = regs_->GetSp();
    frame.fp = regs_->GetFp();
}

bool Unwinder::Impl::CheckFrameValid(const StepFrame& frame, const std::shared_ptr<DfxMap>& map, uintptr_t prevSp)
{
    DFXLOGU("-pc: %{public}p, sp: %{public}p, fp: %{public}p, prevSp: %{public}p", reinterpret_cast<void *>(frame.pc),
        reinterpret_cast<void *>(frame.sp), reinterpret_cast<void *>(frame.fp), reinterpret_cast<void *>(prevSp));
    if (!isFpStep_ && (map != nullptr) && (!map->IsVdsoMap()) && (frame.sp < prevSp)) {
        DFXLOGU("Illegal sp value");
        lastErrorData_.SetAddrAndCode(frame.pc, UNW_ERROR_ILLEGAL_VALUE);
        return false;
    }
    if (frame.pc == 0) {
        return false;
    }
    return true;
}

bool Unwinder::Impl::StepInner(const bool isSigFrame, StepFrame& frame, void *ctx)
{
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        DFXLOGE("params is nullptr");
        return false;
    }
    SetLocalStackCheck(ctx, false);
    DFXLOGU("+pc: %{public}p, sp: %{public}p, fp: %{public}p", reinterpret_cast<void *>(frame.pc),
        reinterpret_cast<void *>(frame.sp), reinterpret_cast<void *>(frame.fp));
    uintptr_t prevSp = frame.sp;
    bool hasRegLocState = false;
    std::shared_ptr<RegLocState> rs = nullptr;
    std::shared_ptr<DfxMap> map = nullptr;
    do {
        // 1. find cache rs
        hasRegLocState = FindCache(frame.pc, map, rs);
        if (hasRegLocState) {
            AddFrame(frame, map);
            break;
        }

        // 2. find map and process frame
        if (!AddFrameMap(frame, map)) {
            if (isCrash_ && !isFpStep_ && !frames_.empty()) {
                auto &lastFrame = frames_.back();
                frame.pc = lastFrame.pc;
                frame.sp = lastFrame.sp;
                frame.fp = lastFrame.fp;
                frame.isJsFrame = lastFrame.isJsFrame;
                DFXLOGW("Dwarf unwind failed to find map, try fp unwind again");
                break;
            }
            return false;
        }
        if (isSigFrame) {
            return true;
        }
        bool stopUnwind = false;
        if (!StepJsvmFrame(frame, map)) {
            break;
        }
        bool processFrameResult = UnwindArkFrame(frame, map, stopUnwind);
        if (stopUnwind) {
            return processFrameResult;
        } else if (!processFrameResult) {
            break;
        }
        if (isFpStep_) {
            if (enableFpCheckMapExec_ && (map != nullptr && !map->IsMapExec())) {
                DFXLOGE("Fp step check map is not exec");
                return false;
            }
            break;
        }
        // 3. find unwind table and entry, parse instructions and get cache rs
        hasRegLocState = ParseUnwindTable(frame.pc, rs);
        // 4. update rs cache
        if (hasRegLocState && enableCache_) {
            StepCache cache;
            cache.map = map;
            cache.rs = rs;
            stepCache_.emplace(frame.pc, cache);
        }
    } while (false);

    // 5. update regs and regs state
    UpdateRegsState(frame, ctx, hasRegLocState, rs);
    return CheckFrameValid(frame, map, prevSp) ? hasRegLocState : false;
}

bool Unwinder::Impl::Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    if (rs == nullptr || regs == nullptr) {
        return false;
    }

    uintptr_t prevPc = regs->GetPc();
    uintptr_t prevSp = regs->GetSp();
    uint16_t errCode = 0;
    bool ret = DfxInstructions::Apply(memory_, *(regs.get()), *(rs.get()), errCode);
    uintptr_t tmp = 0;
    uintptr_t sp = regs->GetSp();
    if (ret && (!memory_->Read<uintptr_t>(sp, &tmp, false))) {
        errCode = UNW_ERROR_UNREADABLE_SP;
        ret = false;
    }
    if (StripPac(regs->GetPc(), pacMask_) == prevPc && regs->GetSp() == prevSp) {
        errCode = UNW_ERROR_REPEATED_FRAME;
        ret = false;
    }
    if (!ret) {
        lastErrorData_.SetCode(errCode);
        DFXLOGU("Failed to apply reg state, errCode: %{public}d", static_cast<int>(errCode));
    }
    return ret;
}

void Unwinder::Impl::DoPcAdjust(uintptr_t& pc)
{
    if (pc <= 0x4) {
        return;
    }
    uintptr_t sz = 0x4;
#if defined(__arm__)
    if ((pc & 0x1) && (memory_ != nullptr)) {
        uintptr_t val;
        if (pc < 0x5 || !(memory_->ReadMem(pc - 0x5, &val)) ||
            (val & 0xe000f000) != 0xe000f000) {
            sz = 0x2;
        }
    }
#elif defined(__x86_64__)
    sz = 0x1;
#endif
    pc -= sz;
}

const std::vector<DfxFrame>& Unwinder::Impl::GetFrames()
{
    if (enableFillFrames_) {
        FillFrames(frames_);
    }
    return frames_;
}

void Unwinder::Impl::AddFrame(const StepFrame& frame, std::shared_ptr<DfxMap> map)
{
#if defined(ENABLE_MIXSTACK)
    if (ignoreMixstack_ && frame.isJsFrame) {
        return;
    }
#endif
    pcs_.emplace_back(frame.pc);
    DfxFrame dfxFrame;
    dfxFrame.isJsFrame = frame.isJsFrame;
    dfxFrame.index = frames_.size();
    dfxFrame.fp = static_cast<uint64_t>(frame.fp);
    dfxFrame.pc = static_cast<uint64_t>(frame.pc);
    dfxFrame.sp = static_cast<uint64_t>(frame.sp);
    dfxFrame.map = map;
    frames_.emplace_back(dfxFrame);
}

void Unwinder::Impl::AddFrame(DfxFrame& frame)
{
    frames_.emplace_back(frame);
}

bool Unwinder::AccessMem(void* memory, uintptr_t addr, uintptr_t *val)
{
    return reinterpret_cast<DfxMemory*>(memory)->ReadMem(addr, val);
}

void Unwinder::FillLocalFrames(std::vector<DfxFrame>& frames)
{
    if (frames.empty()) {
        return;
    }
    auto it = frames.begin();
    while (it != frames.end()) {
        if (dl_iterate_phdr(Unwinder::Impl::DlPhdrCallback, &(*it)) != 1) {
            // clean up frames after first invalid frame
            frames.erase(it, frames.end());
            break;
        }
        it++;
    }
}

void Unwinder::Impl::FillFrames(std::vector<DfxFrame>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i) {
        auto& frame = frames[i];
        if (frame.isJsFrame) {
#if defined(ENABLE_MIXSTACK)
            FillJsFrame(frame);
#endif
        } else {
            FillFrame(frame, enableParseNativeSymbol_);
        }
    }
}

void Unwinder::Impl::ParseFrameSymbol(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        return;
    }
    auto elf = frame.map->GetElf();
    if (elf == nullptr) {
        return;
    }
    if (!DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset)) {
        DFXLOGU("Failed to get symbol, relPc: %{public}" PRIx64 ", mapName: %{public}s",
            frame.relPc, frame.mapName.c_str());
    }
    frame.buildId = elf->GetBuildId();
}

void Unwinder::Impl::FillFrame(DfxFrame& frame, bool needSymParse)
{
    if (frame.map == nullptr) {
        frame.relPc = frame.pc;
        frame.mapName = "Not mapped";
        DFXLOGU("Current frame is not mapped.");
        return;
    }
    frame.mapName = frame.map->GetElfName();
    DFX_TRACE_SCOPED_DLSYM("FillFrame:%s", frame.mapName.c_str());
    frame.relPc = frame.map->GetRelPc(frame.pc);
    frame.mapOffset = frame.map->offset;
    DFXLOGU("mapName: %{public}s, mapOffset: %{public}" PRIx64 "", frame.mapName.c_str(), frame.mapOffset);
    if (needSymParse) {
        ParseFrameSymbol(frame);
    }
}

void Unwinder::Impl::FillJsFrame(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        DFXLOGU("Current js frame is not map.");
        return;
    }
    DFX_TRACE_SCOPED_DLSYM("FillJsFrame:%s", frame.map->name.c_str());
    DFXLOGU("Fill js frame, map name: %{public}s", frame.map->name.c_str());
    JsFunction jsFunction;
    if ((pid_ == UNWIND_TYPE_LOCAL) || (pid_ == UNWIND_TYPE_CUSTOMIZE_LOCAL)) {
        if (!FillJsFrameLocal(frame, &jsFunction)) {
            return;
        }
    } else {
        auto hap = frame.map->GetHap();
        if (hap == nullptr) {
            DFXLOGW("Get hap error, name: %{public}s", frame.map->name.c_str());
            return;
        }
        if (!hap->ParseHapInfo(pid_, frame.pc, frame.map, &jsFunction)) {
            DFXLOGW("Failed to parse hap info, pid: %{public}d", pid_);
            return;
        }
    }
    frame.isJsFrame = true;
    frame.mapName = std::string(jsFunction.url);
    frame.funcName = std::string(jsFunction.functionName);
    frame.packageName = std::string(jsFunction.packageName);
    frame.line = static_cast<int32_t>(jsFunction.line);
    frame.column = jsFunction.column;
    DFXLOGU("Js frame mapName: %{public}s, funcName: %{public}s, line: %{public}d, column: %{public}d",
        frame.mapName.c_str(), frame.funcName.c_str(), frame.line, frame.column);
}

bool Unwinder::Impl::FillJsFrameLocal(DfxFrame& frame, JsFunction* jsFunction)
{
    if (!isArkCreateLocal_) {
        if (DfxArk::Instance().ArkCreateLocal() < 0) {
            DFXLOGW("Failed to ark create local.");
            return false;
        }
        isArkCreateLocal_ = true;
    }

    if (DfxArk::Instance().ParseArkFrameInfoLocal(static_cast<uintptr_t>(frame.pc),
        static_cast<uintptr_t>(frame.map->begin), static_cast<uintptr_t>(frame.map->offset), jsFunction) < 0) {
        DFXLOGW("Failed to parse ark frame info local, pc: %{private}p, begin: %{private}p",
            reinterpret_cast<void *>(frame.pc), reinterpret_cast<void *>(frame.map->begin));
        return false;
    }
    frame.isJsFrame = true;
    return true;
}

bool Unwinder::Impl::GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame &frame)
{
    frame.pc = static_cast<uint64_t>(StripPac(pc, 0));
    std::shared_ptr<DfxMap> map = nullptr;
    if ((maps == nullptr) || !maps->FindMapByAddr(pc, map) || map == nullptr) {
        DFXLOGE("Find map error");
        return false;
    }

    frame.map = map;
    if (DfxMaps::IsArkHapMapItem(map->name) || DfxMaps::IsArkCodeMapItem(map->name)) {
        FillJsFrame(frame);
    } else {
        FillFrame(frame, enableParseNativeSymbol_);
    }
    return true;
}

bool Unwinder::Impl::GetLockInfo(int32_t tid, char* buf, size_t sz)
{
#ifdef __aarch64__
    if (frames_.empty()) {
        return false;
    }

    if (frames_[0].funcName.find("__timedwait_cp") == std::string::npos) {
        return false;
    }

    uintptr_t lockPtrAddr = firstFrameSp_ + 64; // 64 : sp + 0x40
    uintptr_t lockAddr;
    if (ReadProcMemByPid(tid, lockPtrAddr, &lockAddr, sizeof(uintptr_t)) != sizeof(uintptr_t)) {
        DFXLOGW("Failed to find lock addr.");
        return false;
    }

    size_t rsize = ReadProcMemByPid(tid, lockAddr, buf, sz);
    if (rsize != sz) {
        DFXLOGW("Failed to fetch lock content, read size:%{public}zu expected size:%{public}zu", rsize, sz);
        return false;
    }
    return true;
#else
    return false;
#endif
}

void Unwinder::Impl::GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    if (maps_ == nullptr) {
        DFXLOGE("maps_ is null, return directly!");
        return;
    }
    frames.clear();
    std::shared_ptr<DfxMap> map = nullptr;
    for (size_t i = 0; i < pcs.size(); ++i) {
        DfxFrame frame;
        frame.index = i;
        pcs[i] = StripPac(pcs[i], 0);
        frame.pc = static_cast<uint64_t>(pcs[i]);
        if ((map != nullptr) && map->Contain(frame.pc)) {
            DFXLOGU("map had matched");
        } else if (!maps_->FindMapByAddr(pcs[i], map)) {
            map = nullptr;
            frame.relPc = frame.pc;
            frame.mapName = "Not mapped";
            DFXLOGE("Find map error");
            continue;
        }
        frame.map = map;
        if (DfxMaps::IsArkHapMapItem(map->name) || DfxMaps::IsArkCodeMapItem(map->name)) {
            FillJsFrame(frame);
        } else {
            FillFrame(frame, enableParseNativeSymbol_);
        }
        frames.emplace_back(frame);
    }
}

void Unwinder::GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    frames.clear();
    for (size_t i = 0; i < pcs.size(); ++i) {
        DfxFrame frame;
        frame.index = i;
        frame.pc = static_cast<uint64_t>(pcs[i]);
        frames.emplace_back(frame);
    }
    FillLocalFrames(frames);
}

bool Unwinder::GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, std::string& funcName, uint64_t& funcOffset)
{
    if (maps == nullptr) {
        return false;
    }
    std::shared_ptr<DfxMap> map = nullptr;
    if (!maps->FindMapByAddr(pc, map) || (map == nullptr)) {
        DFXLOGE("Find map is null");
        return false;
    }
    uint64_t relPc = map->GetRelPc(static_cast<uint64_t>(pc));
    auto elf = map->GetElf();
    if (elf == nullptr) {
        DFXLOGE("Get elf is null");
        return false;
    }
    return DfxSymbols::GetFuncNameAndOffsetByPc(relPc, elf, funcName, funcOffset);
}

std::string Unwinder::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    return DfxFrameFormatter::GetFramesStr(frames);
}

int Unwinder::Impl::DlPhdrCallback(struct dl_phdr_info *info, size_t size, void *data)
{
    auto frame = reinterpret_cast<DfxFrame *>(data);
    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    frame->pc = StripPac(frame->pc, 0);
    for (int n = info->dlpi_phnum; --n >= 0; phdr++) {
        if (phdr->p_type == PT_LOAD) {
            ElfW(Addr) vaddr = phdr->p_vaddr + info->dlpi_addr;
            if (frame->pc >= vaddr && frame->pc < vaddr + phdr->p_memsz) {
                frame->relPc = frame->pc - info->dlpi_addr;
                frame->mapName = std::string(info->dlpi_name);
                DFXLOGU("relPc: %{public}" PRIx64 ", mapName: %{public}s", frame->relPc, frame->mapName.c_str());
                return 1;
            }
        }
    }
    return 0;
}

bool Unwinder::Impl::UnwindLocalByOtherTid(const pid_t tid, bool fast, size_t maxFrameNum, size_t skipFrameNum)
{
    if (tid < 0 || tid == gettid()) {
        lastErrorData_.SetCode(UNW_ERROR_NOT_SUPPORT);
        return false;
    }
    auto& instance = LocalThreadContextMix::GetInstance();

    if (tid == getpid()) {
        uintptr_t stackBottom = 1;
        uintptr_t stackTop = static_cast<uintptr_t>(-1);
        if (!GetMainStackRangeInner(stackBottom, stackTop)) {
            return false;
        }
        instance.SetStackRang(stackTop, stackBottom);
    }
    bool isSuccess = instance.CollectThreadContext(tid);
    if (!isSuccess) {
        instance.ReleaseCollectThreadContext();
        return false;
    }

    std::shared_ptr<DfxRegs> regs = DfxRegs::Create();
    instance.SetRegister(regs);
    SetRegs(regs);
    EnableFillFrames(true);

    bool ret = false;
    if (fast) {
        maps_ = instance.GetMaps();
        ret = UnwindByFp(&instance, maxFrameNum, 0, true);
    } else {
        ret = Unwind(&instance, maxFrameNum, skipFrameNum);
    }
    instance.ReleaseCollectThreadContext();
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
