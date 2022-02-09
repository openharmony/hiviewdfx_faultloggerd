/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef DFX_PROCESS_DUMP_LOG_H
#define DFX_PROCESS_DUMP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_BUF_LEN 1024

int DfxLogDebug(const char *format, ...);
int DfxLogInfo(const char *format, ...);
int DfxLogWarn(const char *format, ...);
int DfxLogError(const char *format, ...);
int DfxLogFatal(const char *format, ...);
void DfxLogByTrace(bool start, const char *tag); // start : false, finish a tag bytrace; true, start a tag bytrace.
int WriteLog(int fd, const char *format, ...);
void DfxLogToSocket(const char *msg);
void InitDebugLog(int type, int pid, int tid, int uid, bool isLogPersist);
//void InitDebugLog(int type, int pid, int tid, int uid);
void CloseDebugLog(void);
#ifdef __cplusplus
}
#endif

#endif
