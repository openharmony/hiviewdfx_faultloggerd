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

#ifndef DFX_EXCEPTION_H
#define DFX_EXCEPTION_H

#include <inttypes.h>
#include "dfx_define.h"
#include "dfx_socket_request.h"

#ifdef __cplusplus
extern "C" {
#endif

enum CrashExceptionCode : int32_t {
    CRASH_ESUCCESS = 0,                 /* No error */

    CRASH_SIGNAL_EMASKED = 101,         /* Signal has been masked */
    CRASH_SIGNAL_EFORK,                 /* Failed to fork child process */
    CRASH_SIGNAL_ECLONE,                /* Failed to clone thread of recycle dump process */
    CRASH_SIGNAL_ESETSTATE,             /* Failed to set dump state */
    CRASH_SIGNAL_EINHERITCAP,           /* Failed to inherit capabilities */
    CRASH_SIGNAL_EEXECL,                /* Failed to execl processdump */
    CRASH_SIGNAL_EWAITEXIT,             /* Failed to wait vm process exit */
    CRASH_SIGNAL_EREADPIPE,             /* Failed to read pipe due to timeout */
    CRASH_SIGNAL_ECREATEPIPE,           /* Failed to init create pipe */
    CRASH_SIGNAL_EDUMPREQUEST,          /* Failed to find symbol to dump request */

    CRASH_DUMP_EREADREQ = 201,          /* Failed to read dump request */
    CRASH_DUMP_EPARENTPID,              /* Failed to check parent pid */
    CRASH_DUMP_EATTACH,                 /* Failed to attach target process */
    CRASH_DUMP_EWRITEFD,                /* Failed to request writen fd */
    CRASH_DUMP_EKILLED,                 /* Tagert process has been killed */
    CRASH_DUMP_LOCAL_REPORT,            /* Local Handler DumpInfo Report*/
    CRASH_DUMP_EREADPID,                /* Failed to read real pid*/

    CRASH_UNWIND_ECONTEXT = 301,        /* Unwind context illegal */
    CRASH_UNWIND_EFRAME,                /* Failed to step ark js frame */
    CRASH_UNWIND_ESTACK,                /* Stack corruption */

    CRASH_LOG_ESTACKLOS = 401,          /* Crash thread stack not found */
    CRASH_LOG_ECHILDSTACK,              /* Child thread stack not found */
    CRASH_LOG_EREGLOS,                  /* Registers not found */
    CRASH_LOG_EMEMLOS,                  /* Memory not found */
    CRASH_LOG_ESTACKMEMLOS,             /* Fault stack not found */
    CRASH_LOG_EMAPLOS,                  /* Maps not found */
    CRASH_LOG_EHILOGLOS,                /* Hilog not found */
    CRASH_LOG_ESUMMARYLOS,              /* Fault Summary not found */

    CRASH_UNKNOWN = 500,                /* Unknown reason */
};

struct ErrCodeToStr {
    /** Crash exception stage code */
    int32_t errCode;
    /** Crash exception string */
    const char* str;
};

static struct ErrCodeToStr g_crashExceptionMap[] = {
    {CRASH_SIGNAL_EMASKED,      "Signal has been masked." },
    {CRASH_SIGNAL_EFORK,        "Failed to fork child process." },
    {CRASH_SIGNAL_ECLONE,       "Failed to clone thread of recycle dump process." },
    {CRASH_SIGNAL_ESETSTATE,    "Failed to set dump state." },
    {CRASH_SIGNAL_EINHERITCAP,  "Failed to inherit capabilities." },
    {CRASH_SIGNAL_EEXECL,       "Failed to execl processdump." },
    {CRASH_SIGNAL_EWAITEXIT,    "Failed to wait vm process exit." },
    {CRASH_SIGNAL_EREADPIPE,    "Failed to read pipe due to timeout."},
    {CRASH_SIGNAL_ECREATEPIPE,  "Failed to init create pipe."},
    {CRASH_SIGNAL_EDUMPREQUEST, "Failed to find symbol to dump request."},
    {CRASH_DUMP_EREADREQ,       "Failed to read dump request." },
    {CRASH_DUMP_EPARENTPID,     "Failed to check parent pid." },
    {CRASH_DUMP_EATTACH,        "Failed to attach target process." },
    {CRASH_DUMP_EWRITEFD,       "Failed to request writen fd." },
    {CRASH_DUMP_EKILLED,        "Tagert process has been killed." },
    {CRASH_DUMP_EREADPID,       "Failed to read real pid."},
    {CRASH_UNWIND_ECONTEXT,     "Unwind context illegal." },
    {CRASH_UNWIND_EFRAME,       "Failed to step ark js frame." },
    {CRASH_UNWIND_ESTACK,       "Stack corruption." },
    {CRASH_LOG_ESTACKLOS,       "Crash thread stack not found." },
    {CRASH_LOG_ECHILDSTACK,     "Child thread stack not found." },
    {CRASH_LOG_EREGLOS,         "Registers not found." },
    {CRASH_LOG_EMEMLOS,         "Memory not found." },
    {CRASH_LOG_ESTACKMEMLOS,    "Fault stack not found." },
    {CRASH_LOG_EMAPLOS,         "Maps not found." },
    {CRASH_LOG_EHILOGLOS,       "Hilog not found." },
    {CRASH_LOG_ESUMMARYLOS,     "Fault Summary not found."},
    {CRASH_UNKNOWN,             "Unknown reason." },
};

/**
 * @brief Process crash dump exception description
*/
typedef struct CrashDumpException {
    /** request data head **/
    RequestDataHead head;
    /** Crash process id */
    int32_t pid;
    /** Crash process user id */
    int32_t uid;
    /** event happen time */
    int64_t time;
    /** Crash exception error code */
    int32_t error;
    /** Crash exception message */
    char message[LINE_BUF_SIZE];
} __attribute__((packed)) CrashDumpException;;

#ifdef __cplusplus
}
#endif
#endif
