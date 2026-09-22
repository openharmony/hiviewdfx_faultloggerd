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
#ifndef LOCAL_UNWINDER_H
#define LOCAL_UNWINDER_H

#include <cstddef>
#include <link.h>
#include <memory>

#include "unwinder_base.h"

namespace OHOS {
namespace HiviewDFX {

class LocalThreadContextMix;

class LocalUnwinder : public UnwinderBase {
public:
    explicit LocalUnwinder(bool needMaps);

    bool DoUnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum, size_t skipFrameNum) override;
    bool DoUnwindLocalWithTid(const pid_t tid, size_t maxFrameNum, size_t skipFrameNum) override;
    bool DoUnwindLocalByOtherTid(const pid_t tid, bool fast, size_t maxFrameNum, size_t skipFrameNum) override;
    bool __attribute__((optnone)) DoUnwindLocal(bool withRegs, bool fpUnwind, size_t maxFrameNum,
        size_t skipFrameNum, bool enableArk) override;

    void GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs) override;
    void FillLocalFrames(std::vector<DfxFrame>& frames) override;
    static bool FillJsFrame(DfxFrame& frame, JsFunction* jsFunction);

protected:
    void OnDestroy() override;

private:
    static int DlPhdrCallback(struct dl_phdr_info *info, size_t size, void *data);
    bool IsInvalidTid(pid_t tid);
    bool FillPcsByTid(pid_t tid, size_t maxFrameNum, size_t skipFrameNum);
    bool DoUnwindOtherTid(LocalThreadContextMix& instance, bool fast,
        size_t maxFrameNum, size_t skipFrameNum);
    bool PrepareLocalRegs(bool& withRegs, bool fpUnwind);
    void FillLocalContext(uintptr_t stackBottom, uintptr_t stackTop);
    bool UnwindLocalByMode(bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum,
        bool enableArk);
    static bool IsPcInLoadSegment(uintptr_t pc, struct dl_phdr_info *info,
        const ElfW(Phdr)* phdr);
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // LOCAL_UNWINDER_H
