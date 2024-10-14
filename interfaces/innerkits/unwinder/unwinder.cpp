/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#if defined(__arm__)
#include "arm_exidx.h"
#endif
#include "dfx_ark.h"
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_frame_formatter.h"
#include "dfx_hap.h"
#include "dfx_instructions.h"
#include "dfx_log.h"
#include "dfx_memory.h"
#include "dfx_param.h"
#include "dfx_regs_get.h"
#include "dfx_symbols.h"
#include "dfx_trace_dlsym.h"
#include "dwarf_section.h"
#include "fp_unwinder.h"
#include "stack_util.h"
#include "string_printf.h"
#include "string_util.h"
#include "thread_context.h"
#include "elapsed_time.h"

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
        acc_ = std::make_shared<DfxAccessorsLocal>();
        enableFpCheckMapExec_ = true;
        Init();
    }

    // for remote
    Impl(int pid, bool crash) : pid_(pid)
    {
        if (pid <= 0) {
            LOGE("pid(%d) error", pid);
            return;
        }
        DfxEnableTraceDlsym(true);
        maps_ = DfxMaps::Create(pid, crash);
        acc_ = std::make_shared<DfxAccessorsRemote>();
        enableFpCheckMapExec_ = true;
        Init();
    }

    Impl(int pid, int nspid, bool crash) : pid_(nspid)
    {
        if (pid <= 0 || nspid <= 0) {
            LOGE("pid(%d) or nspid(%d) error", pid, nspid);
            return;
        }
        DfxEnableTraceDlsym(true);
        maps_ = DfxMaps::Create(pid, crash);
        acc_ = std::make_shared<DfxAccessorsRemote>();
        enableFpCheckMapExec_ = true;
        Init();
    }

    // for customized
    Impl(const std::shared_ptr<UnwindAccessors> &accessors, bool local)
    {
        if (local) {
            pid_ = UNWIND_TYPE_CUSTOMIZE_LOCAL;
        } else {
            pid_ = UNWIND_TYPE_CUSTOMIZE;
        }
        acc_ = std::make_shared<DfxAccessorsCustomize>(accessors);
        enableFpCheckMapExec_ = false;
        enableFillFrames_ = false;
    #if defined(__aarch64__)
        pacMask_ = pacMaskDefault_;
    #endif
        Init();
    }

    ~Impl()
    {
        DfxEnableTraceDlsym(false);
        Destroy();
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
    inline void EnableMethodIdLocal(bool enableMethodIdLocal)
    {
        enableMethodIdLocal_ = enableMethodIdLocal;
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
    bool UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum);
    bool Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum);

    bool Step(uintptr_t& pc, uintptr_t& sp, void *ctx);
    bool FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx);

    void AddFrame(DfxFrame& frame);
    const std::vector<DfxFrame>& GetFrames();
    inline const std::vector<uintptr_t>& GetPcs() const
    {
        return pcs_;
    }
    void FillFrames(std::vector<DfxFrame>& frames);
    void FillFrame(DfxFrame& frame);
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
    struct StepFrame {
        uintptr_t pc = 0;
        uintptr_t methodid = 0;
        uintptr_t sp = 0;
        uintptr_t fp = 0;
        bool isJsFrame {false};
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
        enableMixstack_ = DfxParam::EnableMixstack();
#endif
    }
    bool GetMainStackRangeInner(uintptr_t& stackBottom, uintptr_t& stackTop);
    bool CheckAndReset(void* ctx);
    void DoPcAdjust(uintptr_t& pc);
    void AddFrame(const StepFrame& frame, std::shared_ptr<DfxMap> map);
    bool StepInner(const bool isSigFrame, StepFrame& frame, void *ctx);
    bool Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);
#if defined(ENABLE_MIXSTACK)
    bool StepArkJsFrame(StepFrame& frame);
#endif
    static uintptr_t StripPac(uintptr_t inAddr, uintptr_t pacMask);
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
    bool enableLrFallback_ = true;
    bool enableFpCheckMapExec_ = false;
    bool enableMethodIdLocal_ = false;
    bool isFpStep_ = false;
    MAYBE_UNUSED bool enableMixstack_ = true;
    MAYBE_UNUSED bool ignoreMixstack_ = false;
    MAYBE_UNUSED bool stopWhenArkFrame_ = false;
    MAYBE_UNUSED bool isJitCrash_ = false;

    int32_t pid_ = 0;
    uintptr_t pacMask_ = 0;
    std::vector<uintptr_t> jitCache_ = {};
    std::shared_ptr<DfxAccessors> acc_ = nullptr;
    std::shared_ptr<DfxMemory> memory_ = nullptr;
    std::unordered_map<uintptr_t, StepCache> stepCache_ {};
    std::shared_ptr<DfxRegs> regs_ = nullptr;
    std::shared_ptr<DfxMaps> maps_ = nullptr;
    std::vector<uintptr_t> pcs_ {};
    std::vector<DfxFrame> frames_ {};
    UnwindErrorData lastErrorData_ {};
#if defined(__arm__)
    std::shared_ptr<ArmExidx> armExidx_ = nullptr;
#endif
    std::shared_ptr<DwarfSection> dwarfSection_ = nullptr;
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
void Unwinder::EnableMethodIdLocal(bool enableMethodIdLocal)
{
    impl_->EnableMethodIdLocal(enableMethodIdLocal);
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

bool Unwinder::UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindLocal(withRegs, fpUnwind, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindRemote(tid, withRegs, maxFrameNum, skipFrameNum);
}

bool Unwinder::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->Unwind(ctx, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindByFp(ctx, maxFrameNum, skipFrameNum);
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

void Unwinder::FillFrame(DfxFrame& frame)
{
    impl_->FillFrame(frame);
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
    memory_ = std::make_shared<DfxMemory>(acc_);
#if defined(__arm__)
    armExidx_ = std::make_shared<ArmExidx>(memory_);
#endif
    dwarfSection_ = std::make_shared<DwarfSection>(memory_);

    InitParam();
#if defined(ENABLE_MIXSTACK)
    LOGD("Unwinder mixstack enable: %d", enableMixstack_);
#else
    LOGD("Unwinder init");
#endif
}

void Unwinder::Impl::Clear()
{
    isFpStep_ = false;
    enableMixstack_ = true;
    pcs_.clear();
    frames_.clear();
    if (memset_s(&lastErrorData_, sizeof(UnwindErrorData), 0, sizeof(UnwindErrorData)) != 0) {
        LOGE("%s", "Failed to memset lastErrorData");
    }
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
    } else if (maps_ == nullptr && !GetMainStackRange(stackBottom, stackTop)) {
        return false;
    }
    return true;
}

bool Unwinder::Impl::GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (gettid() == getpid()) {
        return GetMainStackRangeInner(stackBottom, stackTop);
    }
    return GetSelfStackRange(stackBottom, stackTop);
}

bool Unwinder::Impl::UnwindLocalWithTid(const pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
{
    if (tid < 0 || tid == gettid()) {
        lastErrorData_.SetCode(UNW_ERROR_NOT_SUPPORT);
        LOGE("params is nullptr, tid: %d", tid);
        return false;
    }
    LOGD("UnwindLocalWithTid:: tid: %d", tid);
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
        LOGW("Failed to get thread context of tid(%d)", tid);
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
        LOGE("Failed to get stack range with tid(%d), err(%d)", tid, errno);
        return false;
    }
    if (stackBottom == 0 || stackTop == 0) {
        LOGE("Invalid stack range, err(%d)", errno);
        return false;
    }
    LOGU("stackBottom: %" PRIx64 ", stackTop: %" PRIx64 "", (uint64_t)stackBottom, (uint64_t)stackTop);
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

bool Unwinder::Impl::UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum)
{
    LOGI("UnwindLocal:: fpUnwind: %d", fpUnwind);
    uintptr_t stackBottom = 1;
    uintptr_t stackTop = static_cast<uintptr_t>(-1);
    if (!GetStackRange(stackBottom, stackTop)) {
        LOGE("%s", "Get stack range error");
        return false;
    }
    LOGU("stackBottom: %" PRIx64 ", stackTop: %" PRIx64 "", (uint64_t)stackBottom, (uint64_t)stackTop);

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
                LOGE("%s", "params is nullptr");
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
        return UnwindByFp(&context, maxFrameNum, skipFrameNum);
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
        LOGE("params is nullptr, pid: %d, tid: %d", pid_, tid);
        return false;
    }
    if (tid == 0) {
        tid = pid_;
    }
    if (!withRegs) {
        regs_ = DfxRegs::CreateRemoteRegs(tid);
    }
    if ((regs_ == nullptr)) {
        LOGE("%s", "regs is nullptr");
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
    return DfxArk::JitCodeWriteFile(memory_.get(), &(Unwinder::AccessMem), fd, jitCache_.data(), jitCache_.size());
#else
    return -1;
#endif
}
#if defined(ENABLE_MIXSTACK)
bool Unwinder::Impl::StepArkJsFrame(StepFrame& frame)
{
    DFX_TRACE_SCOPED_DLSYM("StepArkJsFrame pc: %p", reinterpret_cast<void *>(frame.pc));
    std::stringstream timeLimitCheck;
    timeLimitCheck << "StepArkJsFrame, ark pc: " << reinterpret_cast<void *>(frame.pc) <<
        ", fp:" << reinterpret_cast<void *>(frame.fp) << ", sp:" << reinterpret_cast<void *>(frame.sp) <<
        ", isJsFrame:" << frame.isJsFrame;
    ElapsedTime counter(timeLimitCheck.str(), 20); // 20 : limit cost time 20 ms
    if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
        LOGD("+++ark pc: %p, fp: %p, sp: %p, isJsFrame: %d.", reinterpret_cast<void *>(frame.pc),
            reinterpret_cast<void *>(frame.fp), reinterpret_cast<void *>(frame.sp), frame.isJsFrame);
    }
#if defined(ONLINE_MIXSTACK)
    const size_t JSFRAME_MAX = 64;
    JsFrame jsFrames[JSFRAME_MAX];
    size_t size = JSFRAME_MAX;
    if (memset_s(jsFrames, sizeof(JsFrame) * JSFRAME_MAX, 0, sizeof(JsFrame) * JSFRAME_MAX) != 0) {
        LOGE("%s", "Failed to memset_s jsFrames.");
        return false;
    }
    int32_t pid = pid_;
    if (pid_ == UNWIND_TYPE_LOCAL) {
        pid = getpid();
    }
    if (DfxArk::GetArkNativeFrameInfo(pid, frame.pc, frame.fp, frame.sp, jsFrames, size) < 0) {
        LOGE("%s", "Failed to get ark frame info");
        return false;
    }

    if (!ignoreMixstack_) {
        LOGI("---ark js frame size: %zu.", size);
        for (size_t i = 0; i < size; ++i) {
            DfxFrame dfxFrame;
            dfxFrame.isJsFrame = true;
            dfxFrame.index = frames_.size();
            dfxFrame.mapName = std::string(jsFrames[i].url);
            dfxFrame.funcName = std::string(jsFrames[i].functionName);
            dfxFrame.line = jsFrames[i].line;
            dfxFrame.column = jsFrames[i].column;
            AddFrame(dfxFrame);
        }
    }
#else
    int ret = -1;
    uintptr_t *methodId = (pid_ > 0 || enableMethodIdLocal_) ? (&frame.methodid) : nullptr;
    if (isJitCrash_) {
        ArkUnwindParam arkParam(memory_.get(), &(Unwinder::AccessMem), &frame.fp, &frame.sp, &frame.pc,
            methodId, &frame.isJsFrame, jitCache_);
        ret = DfxArk::StepArkFrameWithJit(&arkParam);
    } else {
        ret = DfxArk::StepArkFrame(memory_.get(), &(Unwinder::AccessMem), &frame.fp, &frame.sp, &frame.pc,
            methodId, &frame.isJsFrame);
    }
    if (ret < 0) {
        LOGE("%s", "Failed to step ark frame");
        return false;
    }
    if (pid_ > 0) {
        LOGI("---ark js frame methodid: %" PRIx64 "", (uint64_t)frame.methodid);
    }
#endif
    if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
        LOGD("---ark pc: %p, fp: %p, sp: %p, isJsFrame: %d.", reinterpret_cast<void *>(frame.pc),
            reinterpret_cast<void *>(frame.fp), reinterpret_cast<void *>(frame.sp), frame.isJsFrame);
    }
    return true;
}
#endif

bool Unwinder::Impl::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        LOGE("%s", "params is nullptr?");
        lastErrorData_.SetCode(UNW_ERROR_INVALID_CONTEXT);
        return false;
    }
    SetLocalStackCheck(ctx, false);
    Clear();

    bool needAdjustPc = false;
    bool resetFrames = false;
    StepFrame frame;
    do {
        if (!resetFrames && (skipFrameNum != 0) && (frames_.size() >= skipFrameNum)) {
            resetFrames = true;
            LOGU("frames size: %zu, will be reset frames", frames_.size());
            frames_.clear();
        }
        if (frames_.size() >= maxFrameNum) {
            LOGW("frames size: %zu", frames_.size());
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        frame.pc = regs_->GetPc();
        frame.sp = regs_->GetSp();
        frame.fp = regs_->GetFp();
        // Check if this is a signal frame.
        if (pid_ != UNWIND_TYPE_LOCAL && pid_ != UNWIND_TYPE_CUSTOMIZE_LOCAL &&
            regs_->StepIfSignalFrame(static_cast<uintptr_t>(frame.pc), memory_)) {
            LOGW("Step signal frame, pc: %p", reinterpret_cast<void *>(frame.pc));
            StepInner(true, frame, ctx);
            continue;
        }

        if (!frame.isJsFrame && needAdjustPc) {
            DoPcAdjust(frame.pc);
        }
        needAdjustPc = true;

        uintptr_t prevPc = frame.pc;
        uintptr_t prevSp = frame.sp;
        if (!StepInner(false, frame, ctx)) {
            break;
        }

        if (frame.pc == prevPc && frame.sp == prevSp) {
            if (pid_ >= 0) {
                MAYBE_UNUSED UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
                LOGU("pc and sp is same, tid: %d", uctx->pid);
            } else {
                LOGU("%s", "pc and sp is same");
            }
            lastErrorData_.SetAddrAndCode(frame.pc, UNW_ERROR_REPEATED_FRAME);
            break;
        }
    } while (true);
    LOGU("Last error code: %d, addr: %p", (int)GetLastErrorCode(), reinterpret_cast<void *>(GetLastErrorAddr()));
    LOGU("Last frame size: %zu, last frame pc: %p", frames_.size(), reinterpret_cast<void *>(regs_->GetPc()));
    return (frames_.size() > 0);
}

bool Unwinder::Impl::UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        LOGE("%s", "params is nullptr?");
        return false;
    }
    pcs_.clear();
    bool needAdjustPc = false;

    bool resetFrames = false;
    do {
        if (!resetFrames && skipFrameNum != 0 && (pcs_.size() == skipFrameNum)) {
            LOGU("pcs size: %zu, will be reset pcs", pcs_.size());
            resetFrames = true;
            pcs_.clear();
        }
        if (pcs_.size() >= maxFrameNum) {
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        uintptr_t pc = regs_->GetPc();
        uintptr_t fp = regs_->GetFp();
        if (needAdjustPc) {
            DoPcAdjust(pc);
        }
        needAdjustPc = true;
        pcs_.emplace_back(pc);

        if (!FpStep(fp, pc, ctx) || (pc == 0)) {
            break;
        }
    } while (true);
    return (pcs_.size() > 0);
}

bool Unwinder::Impl::FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx)
{
#if defined(__aarch64__)
    LOGU("+fp: %lx, pc: %lx", (uint64_t)fp, (uint64_t)pc);
    if ((regs_ == nullptr) || (memory_ == nullptr)) {
        LOGE("%s", "params is nullptr");
        return false;
    }
    if (ctx != nullptr) {
        memory_->SetCtx(ctx);
    }

    uintptr_t prevFp = fp;
    uintptr_t ptr = fp;
    if (ptr != 0 && memory_->ReadUptr(ptr, &fp, true) &&
        memory_->ReadUptr(ptr, &pc, false)) {
        if (fp != 0 && fp <= prevFp) {
            LOGU("Illegal or same fp value");
            lastErrorData_.SetAddrAndCode(pc, UNW_ERROR_ILLEGAL_VALUE);
            return false;
        }
        regs_->SetReg(REG_FP, &fp);
        regs_->SetReg(REG_SP, &prevFp);
        regs_->SetPc(StripPac(pc, pacMask_));
        LOGU("-fp: %lx, pc: %lx", (uint64_t)fp, (uint64_t)pc);
        return true;
    }
#endif
    return false;
}

bool Unwinder::Impl::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    DFX_TRACE_SCOPED_DLSYM("Step pc:%p", reinterpret_cast<void *>(pc));
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        LOGE("%s", "params is nullptr?");
        return false;
    }
    bool ret = false;
    StepFrame frame;
    frame.pc = pc;
    frame.sp = sp;
    frame.fp = regs_->GetFp();
    // Check if this is a signal frame.
    if (regs_->StepIfSignalFrame(frame.pc, memory_)) {
        LOGW("Step signal frame, pc: %p", reinterpret_cast<void *>(frame.pc));
        ret = StepInner(true, frame, ctx);
    } else {
        ret = StepInner(false, frame, ctx);
    }
    pc = frame.pc;
    sp = frame.sp;
    return ret;
}

bool Unwinder::Impl::StepInner(const bool isSigFrame, StepFrame& frame, void *ctx)
{
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        LOGE("%s", "params is nullptr");
        return false;
    }
    SetLocalStackCheck(ctx, false);
    LOGU("+pc: %p, sp: %p, fp: %p", reinterpret_cast<void *>(frame.pc),
        reinterpret_cast<void *>(frame.sp), reinterpret_cast<void *>(frame.fp));
    uintptr_t prevSp = frame.sp;

    bool ret = false;
    std::shared_ptr<RegLocState> rs = nullptr;
    std::shared_ptr<DfxMap> map = nullptr;
    do {
        if (enableCache_ && !isFpStep_) {
            // 1. find cache rs
            auto iter = stepCache_.find(frame.pc);
            if (iter != stepCache_.end()) {
                if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                    LOGU("Find rs cache, pc: %p", reinterpret_cast<void *>(frame.pc));
                }
                rs = iter->second.rs;
                map = iter->second.map;
                AddFrame(frame, map);
                ret = true;
                break;
            }
        }

        // 2. find map
        MAYBE_UNUSED int mapRet = acc_->GetMapByPc(frame.pc, map, ctx);
        if (mapRet != UNW_ERROR_NONE) {
            if (frame.isJsFrame) {
                LOGW("Failed to get map with ark, frames size: %zu", frames_.size());
                mapRet = UNW_ERROR_UNKNOWN_ARK_MAP;
            }
            if (frames_.size() > 2) { // 2, least 2 frame
                LOGU("Failed to get map, frames size: %zu", frames_.size());
                lastErrorData_.SetAddrAndCode(frame.pc, mapRet);
                return false;
            }
        }
        AddFrame(frame, map);
        if (isSigFrame) {
            return true;
        }

#if defined(ENABLE_MIXSTACK)
        if (stopWhenArkFrame_ && (map != nullptr && map->IsArkExecutable())) {
            LOGU("Stop by ark frame");
            return false;
        }
        if ((enableMixstack_) && ((map != nullptr && map->IsArkExecutable()) || frame.isJsFrame)) {
            if (!StepArkJsFrame(frame)) {
                LOGE("Failed to step ark Js frame, pc: %p", reinterpret_cast<void *>(frame.pc));
                lastErrorData_.SetAddrAndCode(frame.pc, UNW_ERROR_STEP_ARK_FRAME);
                ret = false;
                break;
            }
            regs_->SetPc(frame.pc);
            regs_->SetSp(frame.sp);
            regs_->SetFp(frame.fp);
#if defined(OFFLINE_MIXSTACK)
            return true;
#endif
        }
#endif
        if (isFpStep_) {
            if (enableFpCheckMapExec_ && (map != nullptr && !map->IsMapExec())) {
                LOGE("%s", "Fp step check map is not exec");
                return false;
            }
            break;
        }

        // 3. find unwind table and entry
        UnwindTableInfo uti;
        MAYBE_UNUSED int utiRet = acc_->FindUnwindTable(frame.pc, uti, ctx);
        if (utiRet != UNW_ERROR_NONE) {
            lastErrorData_.SetAddrAndCode(frame.pc, utiRet);
            LOGU("Failed to find unwind table ret: %d", utiRet);
            break;
        }

        // 4. parse instructions and get cache rs
        struct UnwindEntryInfo uei;
        rs = std::make_shared<RegLocState>();
#if defined(__arm__)
        if (!ret && uti.format == UNW_INFO_FORMAT_ARM_EXIDX) {
            if (!armExidx_->SearchEntry(frame.pc, uti, uei)) {
                lastErrorData_.SetAddrAndCode(armExidx_->GetLastErrorAddr(), armExidx_->GetLastErrorCode());
                LOGE("%s", "Failed to search unwind entry?");
                break;
            }
            if (!armExidx_->Step((uintptr_t)uei.unwindInfo, rs)) {
                lastErrorData_.SetAddrAndCode(armExidx_->GetLastErrorAddr(), armExidx_->GetLastErrorCode());
                LOGU("%s", "Step exidx section error?");
            } else {
                ret = true;
            }
        }
#endif
        if (!ret && uti.format == UNW_INFO_FORMAT_REMOTE_TABLE) {
            if ((uti.isLinear == false && !dwarfSection_->SearchEntry(frame.pc, uti, uei)) ||
                (uti.isLinear == true && !dwarfSection_->LinearSearchEntry(frame.pc, uti, uei))) {
                lastErrorData_.SetAddrAndCode(dwarfSection_->GetLastErrorAddr(), dwarfSection_->GetLastErrorCode());
                LOGU("%s", "Failed to search unwind entry?");
                break;
            }
            memory_->SetDataOffset(uti.segbase);
            if (!dwarfSection_->Step(frame.pc, (uintptr_t)uei.unwindInfo, rs)) {
                lastErrorData_.SetAddrAndCode(dwarfSection_->GetLastErrorAddr(), dwarfSection_->GetLastErrorCode());
                LOGU("%s", "Step dwarf section error?");
            } else {
                ret = true;
            }
        }

        if (ret && enableCache_) {
            // 5. update rs cache
            StepCache cache;
            cache.map = map;
            cache.rs = rs;
            stepCache_.emplace(frame.pc, cache);
            break;
        }
    } while (false);

    // 5. update regs and regs state
    SetLocalStackCheck(ctx, true);
    if (ret) {
#if defined(__arm__) || defined(__aarch64__)
        auto lr = *(regs_->GetReg(REG_LR));
#endif
        ret = Apply(regs_, rs);
#if defined(__arm__) || defined(__aarch64__)
        if (!ret && enableLrFallback_ && (frames_.size() == 1)) {
            regs_->SetPc(lr);
            ret = true;
            if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                LOGW("%s", "Failed to apply first frame, lr fallback");
            }
        }
#endif
    } else {
        if (enableLrFallback_ && (frames_.size() == 1) && regs_->SetPcFromReturnAddress(memory_)) {
            ret = true;
            if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                LOGW("%s", "Failed to step first frame, lr fallback");
            }
        }
    }

#if defined(__aarch64__)
    if (!ret) { // try fp
        ret = FpStep(frame.fp, frame.pc, ctx);
        if (ret && !isFpStep_) {
            if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                LOGI("First enter fp step, pc: %p", reinterpret_cast<void *>(frame.pc));
            }
            isFpStep_ = true;
        }
    }
#endif

    frame.pc = regs_->GetPc();
    frame.sp = regs_->GetSp();
    frame.fp = regs_->GetFp();
    if (!isFpStep_ && (map != nullptr) && (!map->IsVdsoMap()) && (frame.sp < prevSp)) {
        LOGU("Illegal sp value");
        lastErrorData_.SetAddrAndCode(frame.pc, UNW_ERROR_ILLEGAL_VALUE);
        ret = false;
    }
    if (ret && (frame.pc == 0)) {
        ret = false;
    }
    LOGU("-pc: %p, sp: %p, fp: %p, ret: %d", reinterpret_cast<void *>(frame.pc),
        reinterpret_cast<void *>(frame.sp), reinterpret_cast<void *>(frame.fp), ret);
    return ret;
}

bool Unwinder::Impl::Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    if (rs == nullptr || regs == nullptr) {
        return false;
    }

    uint16_t errCode = 0;
    bool ret = DfxInstructions::Apply(memory_, *(regs.get()), *(rs.get()), errCode);
    uintptr_t tmp = 0;
    auto sp = regs->GetSp();
    if (ret && (!memory_->ReadUptr(sp, &tmp, false))) {
        errCode = UNW_ERROR_UNREADABLE_SP;
        ret = false;
    }
    if (!ret) {
        lastErrorData_.SetCode(errCode);
        LOGE("Failed to apply reg state, errCode: %d", static_cast<int>(errCode));
    }

    regs->SetPc(StripPac(regs->GetPc(), pacMask_));
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

uintptr_t Unwinder::Impl::StripPac(uintptr_t inAddr, uintptr_t pacMask)
{
    uintptr_t outAddr = inAddr;
#if defined(__aarch64__)
    if (outAddr != 0) {
        if (pacMask != 0) {
            outAddr &= ~pacMask;
        } else {
            register uint64_t x30 __asm("x30") = inAddr;
            asm("hint 0x7" : "+r"(x30));
            outAddr = x30;
        }
        if (outAddr != inAddr) {
            LOGU("Strip pac in addr: %lx, out addr: %lx", (uint64_t)inAddr, (uint64_t)outAddr);
        }
    }
#endif
    return outAddr;
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
#if defined(OFFLINE_MIXSTACK)
    if (ignoreMixstack_ && frame.isJsFrame) {
        return;
    }
#endif
    pcs_.emplace_back(frame.pc);
    DfxFrame dfxFrame;
    dfxFrame.isJsFrame = frame.isJsFrame;
    dfxFrame.index = frames_.size();
    dfxFrame.pc = static_cast<uint64_t>(frame.pc);
    dfxFrame.sp = static_cast<uint64_t>(frame.sp);
#if defined(OFFLINE_MIXSTACK)
    if (frame.isJsFrame) {
        dfxFrame.funcOffset = static_cast<uint64_t>(frame.methodid);
    }
#endif
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
#if defined(OFFLINE_MIXSTACK)
            FillJsFrame(frame);
#endif
        } else {
            FillFrame(frame);
        }
    }
}

void Unwinder::Impl::FillFrame(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        frame.relPc = frame.pc;
        frame.mapName = "Not mapped";
        LOGU("%s", "Current frame is not mapped.");
        return;
    }
    frame.mapName = frame.map->GetElfName();
    DFX_TRACE_SCOPED_DLSYM("FillFrame:%s", frame.mapName.c_str());
    frame.relPc = frame.map->GetRelPc(frame.pc);
    frame.mapOffset = frame.map->offset;
    LOGU("mapName: %s, mapOffset: %" PRIx64 "", frame.mapName.c_str(), frame.mapOffset);
    auto elf = frame.map->GetElf();
    if (elf == nullptr) {
        if (pid_ == UNWIND_TYPE_LOCAL || pid_ == UNWIND_TYPE_CUSTOMIZE_LOCAL) {
            FillJsFrame(frame);
        }
        return;
    }
    if (!DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset)) {
        LOGU("Failed to get symbol, relPc: %" PRIx64 ", mapName: %s", frame.relPc, frame.mapName.c_str());
    }
    frame.buildId = elf->GetBuildId();
}

void Unwinder::Impl::FillJsFrame(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        LOGU("%s", "Current js frame is not map.");
        return;
    }
    DFX_TRACE_SCOPED_DLSYM("FillJsFrame:%s", frame.map->name.c_str());
    LOGU("Fill js frame, map name: %s", frame.map->name.c_str());
    auto hap = frame.map->GetHap();
    if (hap == nullptr) {
        LOGW("Get hap error, name: %s", frame.map->name.c_str());
        return;
    }
    JsFunction jsFunction;
    if ((pid_ == UNWIND_TYPE_LOCAL) || (pid_ == UNWIND_TYPE_CUSTOMIZE_LOCAL) || enableMethodIdLocal_) {
        if (DfxArk::ParseArkFrameInfoLocal(static_cast<uintptr_t>(frame.pc), static_cast<uintptr_t>(frame.funcOffset),
            static_cast<uintptr_t>(frame.map->begin), static_cast<uintptr_t>(frame.map->offset), &jsFunction) < 0) {
            LOGW("Failed to parse ark frame info local, pc: %p, begin: %p",
                reinterpret_cast<void *>(frame.pc), reinterpret_cast<void *>(frame.map->begin));
            return;
        }
        frame.isJsFrame = true;
    } else {
        if (!hap->ParseHapInfo(pid_, frame.pc, static_cast<uintptr_t>(frame.funcOffset), frame.map, &jsFunction)) {
            LOGW("Failed to parse hap info, pid: %d", pid_);
            return;
        }
    }
    frame.mapName = std::string(jsFunction.url);
    frame.funcName = std::string(jsFunction.functionName);
    frame.line = static_cast<int32_t>(jsFunction.line);
    frame.column = jsFunction.column;
    LOGU("Js frame mapName: %s, funcName: %s, line: %d, column: %d",
        frame.mapName.c_str(), frame.funcName.c_str(), frame.line, frame.column);
}

bool Unwinder::Impl::GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame &frame)
{
    frame.pc = static_cast<uint64_t>(StripPac(pc, 0));
    std::shared_ptr<DfxMap> map = nullptr;
    if ((maps == nullptr) || !maps->FindMapByAddr(pc, map) || map == nullptr) {
        LOGE("%s", "Find map error");
        return false;
    }

    frame.map = map;
    FillFrame(frame);
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

    uintptr_t lockPtrAddr = firstFrameSp_ + 56; // 56 : sp + 0x38
    uintptr_t lockAddr;
    UnwindContext context;
    context.pid = tid;
    if (acc_->AccessMem(lockPtrAddr, &lockAddr, &context) != UNW_ERROR_NONE) {
        LOGW("%s", "Failed to find lock addr.");
        return false;
    }

    size_t rsize = DfxMemory::ReadProcMemByPid(tid, lockAddr, buf, sz);
    if (rsize != sz) {
        LOGW("Failed to fetch lock content, read size:%zu expected size:%zu", rsize, sz);
        return false;
    }
    return true;
#else
    return false;
#endif
}

void Unwinder::Impl::GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    frames.clear();
    std::shared_ptr<DfxMap> map = nullptr;
    for (size_t i = 0; i < pcs.size(); ++i) {
        DfxFrame frame;
        frame.index = i;
        frame.pc = static_cast<uint64_t>(StripPac(pcs[i], 0));
        if ((map != nullptr) && map->Contain(frame.pc)) {
            LOGU("%s", "map had matched");
        } else {
            if ((maps_ == nullptr) || !maps_->FindMapByAddr(pcs[i], map) || (map == nullptr)) {
                LOGE("%s", "Find map error");
            }
        }
        frame.map = map;
        FillFrame(frame);
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
        LOGE("%s", "Find map is null");
        return false;
    }
    uint64_t relPc = map->GetRelPc(static_cast<uint64_t>(pc));
    auto elf = map->GetElf();
    if (elf == nullptr) {
        LOGE("%s", "Get elf is null");
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
                LOGU("relPc: %" PRIx64 ", mapName: %s", frame->relPc, frame->mapName.c_str());
                return 1;
            }
        }
    }
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
