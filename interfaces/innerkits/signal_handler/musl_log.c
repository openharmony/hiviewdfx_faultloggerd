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

#include "musl_log.h"

#include "stdbool.h"

#ifdef ENABLE_SIGHAND_MUSL_LOG
__attribute__ ((visibility("hidden"))) int MuslHiLogPrinter(
    LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = HiLogAdapterPrintArgs(type, level, domain, tag, fmt, ap);
    va_end(ap);
    return ret;
}

__attribute__ ((visibility("hidden"))) int DfxLogPrint(
    const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, ...)
{
    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, fmt);
    ret = vsnprintfp_s(buf, sizeof(buf), sizeof(buf) - 1, false, fmt, args);
    va_end(args);
    MuslHiLogPrinter(LOG_CORE, logLevel, domain, tag, "%{public}s", buf);
    return ret;
}
#endif