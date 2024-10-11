/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef DFX_FAULTLOGGERD_KERNEL_STACK_H
#define DFX_FAULTLOGGERD_KERNEL_STACK_H

#include <cinttypes>
#include <string>
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {
struct DfxThreadStack {
    std::string threadName = "";
    long int tid = 0;
    std::vector<DfxFrame> frames;
};
int32_t DfxGetKernelStack(int32_t pid, std::string& kernelStack);
bool FormatThreadKernelStack(const std::string& kernelStack, DfxThreadStack& threadStack);
bool FormatProcessKernelStack(const std::string& kernelStack, std::vector<DfxThreadStack>& processStack);
}
}
#endif