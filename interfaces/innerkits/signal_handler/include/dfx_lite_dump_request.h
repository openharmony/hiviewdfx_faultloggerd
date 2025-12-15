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
#ifndef DFX_LITE_DUMP_REQUEST_H
#define DFX_LITE_DUMP_REQUEST_H

#include "dfx_dump_request.h"

#ifdef __cplusplus
extern "C" {
#endif
bool IsNoNewPriv(void);
pid_t GetRealPid(void);
bool MMapMemoryOnce();
void UnmapMemoryOnce();
void UpdateSanBoxProcess(struct ProcessDumpRequest *request);
bool CollectStack(struct ProcessDumpRequest *request);
bool CollectStat(struct ProcessDumpRequest *request);
bool CollectStatm(struct ProcessDumpRequest *request);
bool LiteCrashHandler(struct ProcessDumpRequest *request);

bool CollectMemoryNearRegisters(int fd, ucontext_t *context);
bool CollectMaps(const int pipeFd, const char* path);
bool CollectOpenFiles(int pipeWriteFd, const uint64_t fdTableAddr, pid_t pid);
void ResetLiteDump();
#ifdef __cplusplus
}
#endif
#endif