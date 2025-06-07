/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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
#ifndef UNWINDER_H
#define UNWINDER_H

#include <string>
#include <memory>
#include <vector>

#include "dfx_frame.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class Unwinder {
public:
    // for local
    Unwinder(bool needMaps = true);
    // for remote
    Unwinder(int pid, bool crash = true);
    Unwinder(int pid, int nspid, bool crash);
    // for customized
    Unwinder(std::shared_ptr<UnwindAccessors> accessors, bool local = false);
    ~Unwinder() = default;

    // disallow copy and move
    Unwinder(const Unwinder&) = delete;
    Unwinder& operator=(const Unwinder&) = delete;
    Unwinder(Unwinder&&) noexcept = delete;
    Unwinder& operator=(Unwinder&&) noexcept = delete;

    void EnableUnwindCache(bool enableCache);

    void EnableFpCheckMapExec(bool enableFpCheckMapExec);
    void EnableFillFrames(bool enableFillFrames);
    void EnableParseNativeSymbol(bool enableParseNativeSymbol);
    void IgnoreMixstack(bool ignoreMixstack);

    void SetRegs(std::shared_ptr<DfxRegs> regs);
    const std::shared_ptr<DfxRegs>& GetRegs() const;

    const std::shared_ptr<DfxMaps>& GetMaps() const;

    uint16_t GetLastErrorCode() const;
    uint64_t GetLastErrorAddr() const;

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);

    bool UnwindLocalWithContext(const ucontext_t& context,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindLocalWithTid(const pid_t tid,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindLocalByOtherTid(const pid_t tid, bool fast = false,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindLocal(bool withRegs = false, bool fpUnwind = false,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0, bool enableArk = false);
    bool UnwindRemote(pid_t tid = 0, bool withRegs = false,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool Unwind(void *ctx,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindByFp(void *ctx,
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0, bool enableArk = false);

    bool Step(uintptr_t& pc, uintptr_t& sp, void *ctx);
    bool FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx);

    void AddFrame(DfxFrame& frame);
    const std::vector<DfxFrame>& GetFrames() const;
    const std::vector<uintptr_t>& GetPcs() const;
    void FillFrames(std::vector<DfxFrame>& frames);
    void FillFrame(DfxFrame& frame, bool needSymParse = true);
    void ParseFrameSymbol(DfxFrame& frame);
    void FillJsFrame(DfxFrame& frame);
    bool GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame);
    void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs);
    void SetIsJitCrashFlag(bool isCrash);
    int ArkWriteJitCodeToFile(int fd);
    const std::vector<uintptr_t>& GetJitCache(void);
    bool GetLockInfo(int32_t tid, char* buf, size_t sz);
    void SetFrames(std::vector<DfxFrame>& frames);

    static bool GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
        std::string& funcName, uint64_t& funcOffset);
    static void GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs);
    static std::string GetFramesStr(const std::vector<DfxFrame>& frames);
    static void FillLocalFrames(std::vector<DfxFrame>& frames);

    static bool AccessMem(void* memory, uintptr_t addr, uintptr_t *val);
private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
