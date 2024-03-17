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

#ifndef DFX_SIGNALHANDLER_EXCEPTION_H
#define DFX_SIGNALHANDLER_EXCEPTION_H

#include <inttypes.h>

#include "dfx_exception.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief report signal handler exception to faultloggerd
*/
int ReportException(struct CrashDumpException exception);

#ifdef __cplusplus
}
#endif
#endif