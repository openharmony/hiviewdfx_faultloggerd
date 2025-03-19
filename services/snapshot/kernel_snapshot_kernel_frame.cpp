/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "kernel_snapshot_kernel_frame.h"

namespace OHOS {
namespace HiviewDFX {
void KernelFrame::Parse(const std::string& line)
{
    size_t pos = 0;
    pc = ExtractContent(line, pos, '[', ']');
    fp = ExtractContent(line, ++pos, '[', ']');
    funcNameOffset = ExtractContent(line, ++pos, '<', '>');
    elf = ExtractContent(line, ++pos, '(', ')');
}

std::string KernelFrame::ToString(int count) const
{
    return std::string("#") + (count < 10 ? "0" : "") +  // 10 : add leading 0 if less than 10
        std::to_string(count) + " pc " + pc + " " + elf;
}

std::string KernelFrame::ExtractContent(const std::string& line, size_t& pos, char startChar, char endChar)
{
    size_t start = line.find(startChar, pos);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = line.find(endChar, start + 1);
    if (end == std::string::npos) {
        return "";
    }
    return line.substr(start + 1, end - start - 1);
}
} // namespace HiviewDFX
} // namespace OHOS
