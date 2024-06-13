/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#ifndef DFX_ACCESSORS_H
#define DFX_ACCESSORS_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include "dfx_define.h"
#include "unwind_context.h"

#if is_mingw
#ifndef UNWIND_BYTE_ORDER
#define UNWIND_BYTE_ORDER -1 // unknown
#endif
#endif

namespace OHOS {
namespace HiviewDFX {
class DfxMap;

class DfxAccessors {
public:
    DfxAccessors(int bigEndian = UNWIND_BYTE_ORDER) : bigEndian_(bigEndian) {}
    virtual ~DfxAccessors() = default;
    static bool GetMapByPcAndCtx(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg);

    virtual int AccessMem(uintptr_t addr, uintptr_t *val, void *arg) = 0;
    virtual int AccessReg(int regIdx, uintptr_t *val, void *arg) = 0;
    virtual int FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg) = 0;
    virtual int GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg) = 0;

    int bigEndian_ = UNWIND_BYTE_ORDER;
};

class DfxAccessorsLocal : public DfxAccessors {
public:
    DfxAccessorsLocal();
    ~DfxAccessorsLocal();
    bool IsValidFrame(uintptr_t addr, uintptr_t stackBottom, uintptr_t stackTop);

    int AccessMem(uintptr_t addr, uintptr_t *val, void *arg) override;
    int AccessReg(int regIdx, uintptr_t *val, void *arg) override;
    int FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg) override;
    int GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg) override;
private:
    DfxAccessorsLocal(const DfxAccessorsLocal&) = delete;
    DfxAccessorsLocal& operator= (const DfxAccessorsLocal&) = delete;
    int32_t pfd_[PIPE_NUM_SZ] = {-1, -1};
    bool initPipe_ = false;
};

class DfxAccessorsRemote : public DfxAccessors {
public:
    DfxAccessorsRemote() = default;
    virtual ~DfxAccessorsRemote() = default;

    int AccessMem(uintptr_t addr, uintptr_t *val, void *arg) override;
    int AccessReg(int regIdx, uintptr_t *val, void *arg) override;
    int FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg) override;
    int GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg) override;
};

class DfxAccessorsCustomize : public DfxAccessors {
public:
    DfxAccessorsCustomize(std::shared_ptr<UnwindAccessors> accessors) : accessors_(accessors) {}
    virtual ~DfxAccessorsCustomize() = default;

    int AccessMem(uintptr_t addr, uintptr_t *val, void *arg) override;
    int AccessReg(int regIdx, uintptr_t *val, void *arg) override;
    int FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg) override;
    int GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg) override;
private:
    std::shared_ptr<UnwindAccessors> accessors_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
