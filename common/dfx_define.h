/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef DFX_DEFINE_H
#define DFX_DEFINE_H

#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>

#define SIGDUMP 35
#define SIGLOCAL_DUMP 36
#define PROCESSDUMP_TIMEOUT 8
#define NAME_LEN 128
#define PATH_LEN 1024
#define MAX_FATAL_MSG_SIZE 1024

static const int INVALID_FD = -1;

static const int SOCKET_BUFFER_SIZE = 256;
static const char FAULTLOGGERD_SOCK_PATH[] = "/dev/unix/socket/faultloggerd.server";
static const char SERVER_SOCKET_NAME[] = "faultloggerd.server";
static const char PROC_SELF_STATUS_PATH[] = "/proc/self/status";
static const char PROC_SELF_TASK_PATH[] = "/proc/self/task";

#if defined(__arm__)
static const int USER_REG_NUM = 16;
static const int REG_PC_NUM = 15;
static const int REG_LR_NUM = 14;
static const int REG_SP_NUM = 13;
#elif defined(__aarch64__)
static const int USER_REG_NUM = 34;
static const int REG_PC_NUM = 32;
static const int REG_LR_NUM = 30;
static const int REG_SP_NUM = 31;
#elif defined(__x86_64__)
static const int USER_REG_NUM = 27;
static const int REG_PC_NUM = 16;
#endif

static const char PID_STR_NAME[] = "Pid:";
static const char PPID_STR_NAME[] = "PPid:";
static const char NSPID_STR_NAME[] = "NSpid:";

static const int DUMP_CATCHER_NUMBER_ONE = 1;
static const int DUMP_CATCHER_NUMBER_TWO = 2;
static const int DUMP_CATCHER_NUMBER_THREE = 3;
static const int NUMBER_ONE_THOUSAND = 1000;
static const int NUMBER_ONE_MILLION = 1000000;

static const int FAULTSTACK_ITEM_BUFFER_LENGTH = 2048;
static const int FAULTSTACK_SP_REVERSE = 3;
static const int FAULTSTACK_FIRST_FRAME_SEARCH_LENGTH = 64;

// max unwind 64 steps.
static const int BACK_STACK_MAX_STEPS = 64;
// 128K back trace stack size
static const int BACK_STACK_INFO_SIZE = 128 * 1024;

static const int BACK_TRACE_RING_BUFFER_SIZE = 32 * 1024;
static const int BACK_TRACE_RING_BUFFER_PRINT_WAIT_TIME_MS = 10;

static const int SYMBOL_BUF_SIZE = 1024;
static const int LOG_BUF_LEN = 1024;
static const int FILE_WRITE_BUF_LEN = 4096;

static const char TEST_BUNDLE_NAME[] = "com.example.myapplication";
static const char TRUNCATE_TEST_BUNDLE_NAME[] = "e.myapplication";

static const int DUMP_TYPE_NATIVE = -1;
static const int DUMP_TYPE_MIX = -2;
static const int DUMP_TYPE_KERNEL = -3;

#define LIKELY(exp)       (__builtin_expect(!!(exp), true))
#define UNLIKELY(exp)     (__builtin_expect(!!(exp), false))

#define AT_SYMBOL_DEFAULT       __attribute__ ((visibility("default")))
#define AT_SYMBOL_HIDDEN        __attribute__ ((visibility("hidden")))
#define AT_ALWAYS_INLINE        __attribute__((always_inline))
#define ATTRIBUTE_WARN_UNUSED __attribute__((warn_unused_result))
#define ATTRIBUTE_UNUSED    __attribute__((unused))

#define OHOS_TEMP_FAILURE_RETRY(exp)            \
    ({                                          \
    long int _rc;                               \
    do {                                        \
        _rc = (long int)(exp);                  \
    } while ((_rc == -1) && (errno == EINTR));  \
    _rc;                                        \
    })

// keep sync with the definition in hitracec.h
typedef struct TraceInfo {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint64_t valid : 1;
    uint64_t ver : 3;
    uint64_t chainId : 60;

    uint64_t flags : 12;
    uint64_t spanId : 26;
    uint64_t parentSpanId : 26;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint64_t chainId : 60;
    uint64_t ver : 3;
    uint64_t valid : 1;

    uint64_t parentSpanId : 26;
    uint64_t spanId : 26;
    uint64_t flags : 12;
#else
#error "ERROR: No BIG_LITTLE_ENDIAN defines."
#endif
} TraceInfo;

typedef struct ProcInfo {
    int tid;
    int pid;
    int ppid;
    bool ns;
} ProcInfo;

#endif // DFX_DEFINE_H
