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
#ifndef DFX_DUMPREQUEST_H
#define DFX_DUMPREQUEST_H

#include "dfx_dump_request.h"

#ifdef __cplusplus
extern "C" {
#endif

void DfxDumpRequest(int signo, struct ProcessDumpRequest *request);

bool DFX_SetDumpableState(void);
void DFX_RestoreDumpableState(void);

#ifdef __cplusplus
}
#endif
#endif