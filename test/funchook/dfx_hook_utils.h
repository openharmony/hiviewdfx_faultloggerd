/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef DFX_LOG_BACKTRACE_H
#define DFX_LOG_BACKTRACE_H

#define GEN_HOOK_FUNC(GenFuncName, RealHookFunc, FuncName, RealFuncName) \
static void GenFuncName(void) \
{ \
    RealFuncName = ((RealHookFunc)dlsym(RTLD_NEXT, FuncName)); \
    if (RealFuncName != NULL) { \
        return; \
    } \
    LOGERROR("Failed to find hooked %{public}s use RTLD_NEXT\n", FuncName); \
    RealFuncName = (RealHookFunc)dlsym(RTLD_DEFAULT, FuncName); \
    if (RealFuncName != NULL) { \
        return; \
    } \
    LOGERROR("Failed to find hooked %{public}s use RTLD_DEFAULT\n", FuncName); \
}

#ifdef __cplusplus
extern "C" {
#endif
void LogBacktrace(void);
#ifdef __cplusplus
}
#endif
#endif
