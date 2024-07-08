 /*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef __DFX_TRACE_DLSYM_H__
#define __DFX_TRACE_DLSYM_H__

#include <cstdlib>
#include <cstdint>
namespace OHOS {
namespace HiviewDFX {
void DfxEnableTraceDlsym(bool enableTrace);
#ifdef DFX_ENABLE_TRACE
void DfxStartTraceDlsym(const char* value);
void DfxFinishTraceDlsym();

class DfxTraceScoped {
public:
    DfxTraceScoped(const char* value)
    {
        DfxStartTraceDlsym(value);
    }
    DfxTraceScoped(const DfxTraceScoped&) = delete;
    DfxTraceScoped& operator=(const DfxTraceScoped&) = delete;

    ~DfxTraceScoped()
    {
        DfxFinishTraceDlsym();
    }
};
constexpr int TRACE_BUF_LEN = 128;
void DfxStartTraceDlsymFormat(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void DfxStartTraceDlsym(const char* value);
void DfxFinishTraceDlsym();
void FormatTraceName(char *name, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#define DFX_TRACE_SCOPED_DLSYM(fmt, ...) \
    char traceName[TRACE_BUF_LEN] = {0}; \
    FormatTraceName(traceName, TRACE_BUF_LEN, fmt, ##__VA_ARGS__); \
    DfxTraceScoped trace(traceName)

#define DFX_TRACE_START_DLSYM(fmt, ...) DfxStartTraceDlsymFormat(fmt, ##__VA_ARGS__)
#define DFX_TRACE_FINISH_DLSYM(...) DfxFinishTraceDlsym()
#else
#define DFX_TRACE_SCOPED_DLSYM(fmt, ...)
#define DFX_TRACE_START_DLSYM(fmt, ...)
#define DFX_TRACE_FINISH_DLSYM(...)
#endif
} // namespace HiviewDFX
} // namespace OHOS
#endif