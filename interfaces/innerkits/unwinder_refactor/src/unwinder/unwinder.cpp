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
#include "unwinder_refactor.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dfx_frame.h"
#include "dfx_frame_formatter.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "unwind_define.h"

#include "customized_unwinder.h"
#include "local_unwinder.h"
#include "remote_unwinder.h"

namespace OHOS {
namespace HiviewDFX {

class Unwinder::Impl {
public:
    explicit Impl(bool needMaps)
        : strategy_(std::make_unique<LocalUnwinder>(needMaps))
    {}

    Impl(int pid, bool crash, std::shared_ptr<DfxMaps> maps)
        : strategy_(std::make_unique<RemoteUnwinder>(pid, crash, maps))
    {}

    Impl(int pid, int nspid, bool crash, std::shared_ptr<DfxMaps> maps)
        : strategy_(std::make_unique<RemoteUnwinder>(pid, nspid, crash, maps))
    {}

    Impl(std::shared_ptr<UnwindAccessors> accessors, bool local)
        : strategy_(std::make_unique<CustomizedUnwinder>(accessors, local))
    {}

    ~Impl() = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    void EnableUnwindCache(bool enableCache)
    {
        strategy_->EnableUnwindCache(enableCache);
    }

    void EnableFpCheckMapExec(bool enableFpCheckMapExec)
    {
        strategy_->EnableFpCheckMapExec(enableFpCheckMapExec);
    }

    void EnableFillFrames(bool enableFillFrames)
    {
        strategy_->EnableFillFrames(enableFillFrames);
    }

    void EnableParseNativeSymbol(bool enableParseNativeSymbol)
    {
        strategy_->EnableParseNativeSymbol(enableParseNativeSymbol);
    }

    void EnableJsvmstack(bool enableJsvmstack)
    {
        strategy_->EnableJsvmstack(enableJsvmstack);
    }

    void IgnoreMixstack(bool ignoreMixstack)
    {
        strategy_->IgnoreMixstack(ignoreMixstack);
    }

    void SetRegs(std::shared_ptr<DfxRegs> regs)
    {
        strategy_->SetRegs(regs);
    }

    void SetMaps(std::shared_ptr<DfxMaps> maps)
    {
        strategy_->SetMaps(maps);
    }

    const std::shared_ptr<DfxRegs>& GetRegs() const
    {
        return strategy_->GetRegs();
    }

    const std::shared_ptr<DfxMaps>& GetMaps() const
    {
        return strategy_->GetMaps();
    }

    uint16_t GetLastErrorCode() const
    {
        return strategy_->GetLastErrorCode();
    }

    uint64_t GetLastErrorAddr() const
    {
        return strategy_->GetLastErrorAddr();
    }

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
    {
        return strategy_->GetStackRange(stackBottom, stackTop);
    }

    bool UnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum, size_t skipFrameNum)
    {
        return strategy_->DoUnwindLocalWithContext(context, maxFrameNum, skipFrameNum);
    }

    bool UnwindLocalWithTid(const pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
    {
        return strategy_->DoUnwindLocalWithTid(tid, maxFrameNum, skipFrameNum);
    }

    bool UnwindLocalByOtherTid(const pid_t tid, bool fast, size_t maxFrameNum, size_t skipFrameNum)
    {
        return strategy_->DoUnwindLocalByOtherTid(tid, fast, maxFrameNum, skipFrameNum);
    }

    bool __attribute__((optnone)) UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum,
        size_t skipFrameNum, bool enableArk)
    {
        return strategy_->DoUnwindLocal(withRegs, fpUnwind, maxFrameNum, skipFrameNum + 1, enableArk);
    }

    bool UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
    {
        return strategy_->DoUnwindRemote(tid, withRegs, maxFrameNum, skipFrameNum);
    }

    bool Unwind(void* ctx, size_t maxFrameNum, size_t skipFrameNum)
    {
        return strategy_->Unwind(ctx, maxFrameNum, skipFrameNum);
    }

    bool UnwindByFp(void* ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
    {
        return strategy_->UnwindByFp(ctx, maxFrameNum, skipFrameNum, enableArk);
    }

    bool Step(uintptr_t& pc, uintptr_t& sp, void* ctx)
    {
        return strategy_->Step(pc, sp, ctx);
    }

    bool FpStep(uintptr_t& fp, uintptr_t& pc, void* ctx)
    {
        return strategy_->FpStep(fp, pc, ctx);
    }

    void AddFrame(DfxFrame& frame)
    {
        strategy_->AddFrame(frame);
    }

    const std::vector<DfxFrame>& GetFrames()
    {
        return strategy_->GetFrames();
    }

    const std::vector<uintptr_t>& GetPcs() const
    {
        return strategy_->GetPcs();
    }

    void FillFrames(std::vector<DfxFrame>& frames)
    {
        strategy_->FillFrames(frames);
    }

    void FillFrame(DfxFrame& frame, bool needSymParse)
    {
        strategy_->FillFrame(frame, needSymParse);
    }

    void ParseFrameSymbol(DfxFrame& frame)
    {
        strategy_->ParseFrameSymbol(frame);
    }

    void FillJsFrame(DfxFrame& frame)
    {
        strategy_->FillJsFrame(frame);
    }

    bool GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame)
    {
        return strategy_->GetFrameByPc(pc, maps, frame);
    }

    void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
    {
        strategy_->GetFramesByPcs(frames, std::move(pcs));
    }

    void SetIsJitCrashFlag(bool isCrash)
    {
        strategy_->SetIsJitCrashFlag(isCrash);
    }

    int ArkWriteJitCodeToFile(int fd)
    {
        return strategy_->ArkWriteJitCodeToFile(fd);
    }

    const std::vector<uintptr_t>& GetJitCache() const
    {
        return strategy_->GetJitCache();
    }

    bool GetLockInfo(int32_t tid, char* buf, size_t sz)
    {
        return strategy_->GetLockInfo(tid, buf, sz);
    }

    void SetFrames(std::vector<DfxFrame>& frames)
    {
        strategy_->SetFrames(frames);
    }

    void GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
    {
        strategy_->GetLocalFramesByPcs(frames, std::move(pcs));
    }

    void FillLocalFrames(std::vector<DfxFrame>& frames)
    {
        strategy_->FillLocalFrames(frames);
    }

    bool GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
        std::string& funcName, uint64_t& funcOffset)
    {
        return strategy_->GetSymbolByPc(pc, maps, funcName, funcOffset);
    }

private:
    std::unique_ptr<UnwinderBase> strategy_;
};

Unwinder::Unwinder(bool needMaps)
    : impl_(std::make_shared<Impl>(needMaps))
{}

Unwinder::Unwinder(int pid, bool crash, std::shared_ptr<DfxMaps> maps)
    : impl_(std::make_shared<Impl>(pid, crash, maps))
{}

Unwinder::Unwinder(int pid, int nspid, bool crash, std::shared_ptr<DfxMaps> maps)
    : impl_(std::make_shared<Impl>(pid, nspid, crash, maps))
{}

Unwinder::Unwinder(std::shared_ptr<UnwindAccessors> accessors, bool local)
    : impl_(std::make_shared<Impl>(accessors, local))
{}

Unwinder::~Unwinder() = default;

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

void Unwinder::SetMaps(std::shared_ptr<DfxMaps> maps)
{
    impl_->SetMaps(maps);
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

bool Unwinder::UnwindLocalWithTid(const pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindLocalWithTid(tid, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindLocalByOtherTid(const pid_t tid, bool fast, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindLocalByOtherTid(tid, fast, maxFrameNum, skipFrameNum);
}

bool __attribute__((optnone)) Unwinder::UnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum,
    size_t skipFrameNum, bool enableArk)
{
    return impl_->UnwindLocal(withRegs, fpUnwind, maxFrameNum, skipFrameNum, enableArk);
}

bool Unwinder::UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->UnwindRemote(tid, withRegs, maxFrameNum, skipFrameNum);
}

bool Unwinder::Unwind(void* ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    return impl_->Unwind(ctx, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindByFp(void* ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
{
    return impl_->UnwindByFp(ctx, maxFrameNum, skipFrameNum, enableArk);
}

bool Unwinder::Step(uintptr_t& pc, uintptr_t& sp, void* ctx)
{
    return impl_->Step(pc, sp, ctx);
}

bool Unwinder::FpStep(uintptr_t& fp, uintptr_t& pc, void* ctx)
{
    return impl_->FpStep(fp, pc, ctx);
}

void Unwinder::AddFrame(DfxFrame& frame)
{
    impl_->AddFrame(frame);
}

const std::vector<DfxFrame>& Unwinder::GetFrames()
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
    impl_->GetFramesByPcs(frames, std::move(pcs));
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

bool Unwinder::GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
    std::string& funcName, uint64_t& funcOffset)
{
    return impl_->GetSymbolByPc(pc, maps, funcName, funcOffset);
}

void Unwinder::GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    impl_->GetLocalFramesByPcs(frames, std::move(pcs));
}

std::string Unwinder::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    return DfxFrameFormatter::GetFramesStr(frames);
}

void Unwinder::FillLocalFrames(std::vector<DfxFrame>& frames)
{
    impl_->FillLocalFrames(frames);
}

bool Unwinder::AccessMem(void* memory, uintptr_t addr, uintptr_t* val)
{
    return UnwindCore::AccessMem(memory, addr, val);
}

} // namespace HiviewDFX
} // namespace OHOS
