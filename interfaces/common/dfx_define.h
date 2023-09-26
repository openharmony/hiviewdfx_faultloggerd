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
#define SIGLEAK_STACK 42
#define PROCESSDUMP_TIMEOUT 8
#define DUMPCATCHER_TIMEOUT 15

#ifndef NAME_MAX
#define NAME_MAX         255
#endif
#ifndef PATH_MAX
#define PATH_MAX        1024
#endif
#define NAME_LEN        128
#define PATH_LEN        512
#define LINE_BUF_SIZE   1024
#define MAX_FATAL_MSG_SIZE  1024
static const int NUMBER_ONE_THOUSAND = 1000;
static const int NUMBER_ONE_MILLION = 1000000;

static const int INVALID_FD = -1;
static const int DUMP_TYPE_NATIVE = -1;
static const int DUMP_TYPE_MIX = -2;
static const int DUMP_TYPE_KERNEL = -3;

static const int SOCKET_BUFFER_SIZE = 256;
static const char FAULTLOGGERD_SOCK_BASE_PATH[] = "/dev/unix/socket/";
static const char SERVER_SOCKET_NAME[] = "faultloggerd.server";
static const char SERVER_CRASH_SOCKET_NAME[] = "faultloggerd.crash.server";

static const char PROC_SELF_STATUS_PATH[] = "/proc/self/status";
static const char PROC_SELF_TASK_PATH[] = "/proc/self/task";
static const char PROC_SELF_CMDLINE_PATH[] = "/proc/self/cmdline";
static const char PROC_SELF_COMM_PATH[] = "/proc/self/comm";
static const char PROC_SELF_MAPS_PATH[] = "/proc/self/maps";
static const char PROC_SELF_EXE_PATH[] = "/proc/self/exe";

static const char TEST_BUNDLE_NAME[] = "com.example.myapplication";
static const char TRUNCATE_TEST_BUNDLE_NAME[] = "e.myapplication";

#define LIKELY(exp)       (__builtin_expect(!!(exp), true))
#define UNLIKELY(exp)     (__builtin_expect(!!(exp), false))

#define AT_SYMBOL_DEFAULT       __attribute__ ((visibility("default")))
#define AT_SYMBOL_HIDDEN        __attribute__ ((visibility("hidden")))
#define AT_ALWAYS_INLINE        __attribute__((always_inline))
#define AT_WARN_UNUSED          __attribute__((warn_unused_result))
#define AT_UNUSED               __attribute__((unused))
#define MAYBE_UNUSED            [[maybe_unused]]

#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED [[clang::fallthrough]]  // NOLINT
#endif

#define OHOS_TEMP_FAILURE_RETRY(exp)            \
    ({                                          \
    long int _rc;                               \
    do {                                        \
        _rc = (long int)(exp);                  \
    } while ((_rc == -1) && (errno == EINTR));  \
    _rc;                                        \
    })

#endif
