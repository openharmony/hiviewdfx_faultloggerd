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
#ifdef DFX_ENABLE_TRACE
#include "dfx_trace.h"
#include <cstdio>
#include <cstdarg>
#include <securec.h>
void DfxStartTrace(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char traceName[TRACE_BUF_LEN] = {0};
    int ret = vsnprintf_s(traceName, sizeof(traceName), sizeof(traceName) - 1, fmt, args);
    va_end(args);
    if (ret == -1) {
        strcpy_s(traceName, TRACE_BUF_LEN, "DefaultTraceName");
    }
    StartTrace(HITRACE_TAG_APP, traceName);
}

void FormatTraceName(char *name, size_t size, const char *fmt, ...)
{
    if (size < 1 || name == nullptr) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf_s(name, size, size - 1, fmt, args);
    va_end(args);
    std::string traceName = "DefaultTraceName";
    if (ret == -1 && size > traceName.length()) {
        ret = strcpy_s(name, size, traceName.c_str());
        if (ret != 0) {
            return;
        }
    }
}
#endif