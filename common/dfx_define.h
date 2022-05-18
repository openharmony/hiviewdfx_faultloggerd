/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

static const int NAME_LEN = 128;

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

static const int ARM_EXEC_STEP_NORMAL = 4;
static const int ARM_EXEC_STEP_THUMB = 3;

static const int CONF_LINE_SIZE = 1024;

static const int FAULTSTACK_ITEM_BUFFER_LENGTH = 2048;
static const int FAULTSTACK_SP_REVERSE = 3;
static const int FAULTSTACK_FIRST_FRAME_SEARCH_LENGTH = 64;


// max unwind 64 steps.
static const int BACK_STACK_MAX_STEPS = 64;
// 128K back trace stack size
static const int BACK_STACK_INFO_SIZE = 128 * 1024;

static const int SIGDUMP = 35;

static const int DUMP_CATCHER_NUMBER_ONE = 1;
static const int DUMP_CATCHER_NUMBER_TWO = 2;
static const int DUMP_CATCHER_NUMBER_THREE = 3;
static const int DUMP_CATCHER_SLEEP_TIME_TEN_S = 10;
static const int DUMP_CATCHER_SLEEP_TIME_TWENTY_S = 20;
static const int DUMP_CATCHER_WAIT_LOG_FILE_GEN_TIME_US = 10000;
static const int DUMP_CATCHE_WORK_TIME_S = 60;

static const int NUMBER_TWO_KB = 2048;

static const int BACK_TRACE_RING_BUFFER_SIZE = 32 * 1024;
static const int BACK_TRACE_RING_BUFFER_PRINT_WAIT_TIME_MS = 10;

static const int LOG_BUF_LEN = 1024;
static const int FILE_WRITE_BUF_LEN = 4096;

static const int PROCESSDUMP_TIMEOUT = 30;

static const int REGS_PRINT_LEN_ARM = 256;
static const int REGS_PRINT_LEN_ARM64 = 1024;
static const int REGS_PRINT_LEN_X86 = 512;

static const int PERFORMANCE_TEST_NUMBER_ONE_HUNDRED = 100;
static const double PERFORMANCE_TEST_MAX_UNWIND_TIME_S = 0.03;

#endif
