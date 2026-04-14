/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef _HMUAPI_PDUMP_H_
#define _HMUAPI_PDUMP_H_

#include <linux/types.h>
#include <linux/ioctl.h>
#include <unistd.h>

#define __PDUMP_MAX_COMM_LEN 64

enum __minidump_stream_type {
	__MINIDUMP_TYPE_START__ = 0,
	__MINIDUMP_TYPE_SYSTEM_INFO = __MINIDUMP_TYPE_START__,
	__MINIDUMP_TYPE_EXCEPTION,
	__MINIDUMP_TYPE_THREAD_NAME,
	__MINIDUMP_TYPE_THREAD_LIST,
	__MINIDUMP_TYPE_MEMORY_LIST,
	__MINIDUMP_TYPE_MODULE_LIST,
	__MINIDUMP_TYPE_MEMORY_INFO_LIST,
	__MINIDUMP_TYPE_MISC_INFO,
	__MINIDUMP_TYPE_MAX__
};

#define __MINIDUMP_STREAM_TYPE_FLAG(stream_type) (1ULL << (stream_type))
#define __DEFAULT_MINIDUMP_STREAM_FLAGS \
	(__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_SYSTEM_INFO) | \
	__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_MISC_INFO) | \
	__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_THREAD_NAME) | \
	__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_THREAD_LIST) | \
	__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_MEMORY_LIST) | \
	__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_MODULE_LIST) | \
	__MINIDUMP_STREAM_TYPE_FLAG(__MINIDUMP_TYPE_EXCEPTION))

enum __pdump_data_type {
	__DATA_TYPE_WORK_START = 0,
	__DATA_TYPE_WORK_END,
};

enum __pdump_dump_type {
	__PDUMP_TYPE_INVALID = 0U,
	__PDUMP_TYPE_CALLSTACK,
	__PDUMP_TYPE_MINIDUMP,
	__PDUMP_TYPE_COREDUMP,
	__PDUMP_TYPE_MAX,
};

#define __PDUMP_TYPE_FLAG_CALLSTACK (1U << __PDUMP_TYPE_CALLSTACK)
#define __PDUMP_TYPE_FLAG_MINIDUMP (1U << __PDUMP_TYPE_MINIDUMP)
#define __PDUMP_TYPE_FLAG_COREDUMP (1U << __PDUMP_TYPE_COREDUMP)
#define __PDUMP_TYPE_FLAG_ALL (__PDUMP_TYPE_FLAG_CALLSTACK | \
		__PDUMP_TYPE_FLAG_MINIDUMP | \
		__PDUMP_TYPE_FLAG_COREDUMP)
#define __pdump_is_type_flag_valid(flags) ((((flags) & (~__PDUMP_TYPE_FLAG_ALL)) == 0) && \
		(((flags) & __PDUMP_TYPE_FLAG_ALL) != 0))
#define __pdump_dump_type_to_flag(dump_type) (1U << (dump_type))

struct __pdump_header_s {
	enum __pdump_data_type data_type;
	unsigned int workid;
};

struct __pdump_work_data_s {
	/* type of enum __pdump_dump_type */
	unsigned int dump_type;
	/* pid of the target process */
	pid_t pid;
	/* the pipe fd for reading out data */
	int pipefd;
	int reserved;
};

struct __pdump_work_result_s {
	/* dump type of the work, value of enum __pdump_dump_type */
	unsigned int dump_type;
	/* posix errcode of the finished work */
	int errcode;
	/* total bytes of data written to pipe */
	unsigned int output_bytes;
	int reserved;
};

struct __pdump_data_s {
	struct __pdump_header_s header;
	union {
		struct __pdump_work_data_s work_data;
		struct __pdump_work_result_s result_data;
	} data;
};

struct __pdump_minidump_init_config_s {
	/* streams to dump for session, bitwise flags of enum __minidump_stream_type */
	__u64 target_stream_flags;
};

struct __pdump_coredump_init_config_s {
	__u64 coredump_filter;
};

struct __pdump_init_config_s {
	/* maxnium number of threads to dump for one process */
	__u32 nr_threads_max;
	__u32 reserved;

	struct __pdump_minidump_init_config_s minidump_config;
	struct __pdump_coredump_init_config_s coredump_config;
};

#define __PDUMP_PID_NO_RESTRICT ((pid_t)-1)
#define __PDUMP_PID_FROM_CONFIG ((pid_t)0)
struct __pdump_init_arg_s {
	/* flags of __PDUMP_TYPE_FLAG_XXX */
	unsigned int dump_type_flag;
	/*
	 * pid of target process to dump,
	 * when specified as -1 (__PDUMP_PID_NO_RESTRICT), any processes will be dumped,
	 * when specified as 0 (__PDUMP_PID_FROM_CONFIG), only the processes configured
	 * through __PDUMP_IOCTL_SET_DUMPABLE will be dumped, otherwise,
	 * only the process matches this pid will be dumped
	 */
	pid_t target_pid;
	/* only dump children processes */
	bool children_only;
	/* comm of the target processes */
	char target_comm[__PDUMP_MAX_COMM_LEN];
	/* set pipefd to nonblock mode */
	bool read_nonblock;

	struct __pdump_init_config_s config;
};

struct __pdump_livedump_arg_s {
	/* tid of the target process to dump on live */
	pid_t target_tid;
};

struct __pdump_work_cancel_arg_s {
	/* the workid to be canceled within the session */
	unsigned int workid;
};

struct __pdump_dumpable_arg_s {
	/* process of this pid can be dumped by this session */
	pid_t target_pid;
};

#define __PDUMP_IOCTL_INIT 		_IOW('p', 1, struct __pdump_init_arg_s)
#define __PDUMP_IOCTL_SET_DUMPABLE	_IOR('p', 2, struct __pdump_dumpable_arg_s)
#define __PDUMP_IOCTL_CLEAR_DUMPABLE	_IOR('p', 3, struct __pdump_dumpable_arg_s)
#define __PDUMP_IOCTL_CANCEL		_IOR('p', 4, struct __pdump_work_cancel_arg_s)
#define __PDUMP_IOCTL_LIVEDUMP		_IOR('p', 5, struct __pdump_livedump_arg_s)

#endif
