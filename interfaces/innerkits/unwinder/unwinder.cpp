/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <pthread.h>
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dwarf_unwinder.h"
#include "fp_unwinder.h"
#include "qut_unwinder.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwinder"
}

Unwinder::~Unwinder()
{
    Destroy();
}

void Unwinder::SetTargetPid(int pid)
{
    if (unwinder_ != nullptr) {
        auto context = unwinder_->GetContext();
        if (context != nullptr) {
            context->as->pid = pid;
        }
    }
}

uintptr_t Unwinder::GetPcAdjustment()
{
    return 0;
}

void Unwinder::Destroy()
{
    if (unwinder_ != nullptr) {
        unwinder_.reset();
        unwinder_ = nullptr;
    }
}

bool Unwinder::Init()
{
    Destroy();

    if (mode_ == UnwindMode::DWARF_UNWIND) {
        unwinder_ = std::make_shared<DwarfUnwinder>();
    } else if (mode_ == UnwindMode::FRAMEPOINTER_UNWIND) {
        unwinder_ = std::make_shared<FpUnwinder>();
    } else if (mode_ == UnwindMode::QUICKEN_UNWIND) {
        unwinder_ = std::make_shared<QutUnwinder>();
    } else {
        return false;
    }
    return true;
}

int Unwinder::UnwindInitLocal(UnwindContext* context)
{
    int ret = UNW_ERROR_NONE;
    if (Init() == false) {
        return UNW_ERROR_INVALID_MODE;
    }
    context->as->pid = -1;
    unwinder_->UnwindInit(context);
    return ret;
}

int Unwinder::UnwindInitRemote(UnwindContext* context)
{
    int ret = UNW_ERROR_NONE;
    if (Init() == false) {
        return UNW_ERROR_INVALID_MODE;
    }
    unwinder_->UnwindInit(context);
    return ret;
}

int Unwinder::Unwind(std::vector<DfxFrame> &frames)
{
    int ret = UNW_ERROR_NONE;
    if (unwinder_->Unwind() == false) {
        ret = static_cast<int>(unwinder_->GetLastErrorCode());
    }
    frames = unwinder_->GetFrames();
    return ret;
}

int Unwinder::SearchUnwindTable(uintptr_t pc, UnwindDynInfo *di, UnwindProcInfo *pi, int needUnwindInfo, void *arg)
{
    return 0;
}

bool Unwinder::GetSymbolInfo(uint64_t pc, std::string* funcName, uint64_t* symStart, uint64_t* symEnd)
{
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
