/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

/* This files contains process dump arm reg module. */

#include "dfx_regs.h"

#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
std::shared_ptr<DfxRegs> DfxRegs::Create()
{
    std::shared_ptr<DfxRegs> regs;
#if defined(__arm__)
    regs = std::make_shared<DfxRegsArm>();
#elif defined(__aarch64__)
    regs = std::make_shared<DfxRegsArm64>();
#elif defined(__x86_64__)
    regs = std::make_shared<DfxRegsX86_64>();
#else
#error "Unsupported architecture"
#endif
    return regs;
}

std::shared_ptr<DfxRegs> DfxRegs::CreateFromContext(const ucontext_t &context)
{
    std::shared_ptr<DfxRegs> regs;
#if defined(__arm__)
    regs = std::make_shared<DfxRegsArm>(context);
#elif defined(__aarch64__)
    regs = std::make_shared<DfxRegsArm64>(context);
#elif defined(__x86_64__)
    regs = std::make_shared<DfxRegsX86_64>(context);
#else
#error "Unsupported architecture"
#endif
    return regs;
}

std::vector<uintptr_t> DfxRegs::GetRegsData() const
{
    return regsData_;
}

void DfxRegs::SetRegsData(const std::vector<uintptr_t>& regs)
{
    regsData_ = regs;
}

int DfxRegs::PrintFormat(char *buf, int size, const char *format, ...) const
{
    int ret = -1;
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, size, size - 1, format, args);
    va_end(args);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
