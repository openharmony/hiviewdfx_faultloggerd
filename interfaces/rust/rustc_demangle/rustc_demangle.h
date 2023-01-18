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

#ifndef RUSTC_DEMANGLE_H
#define RUSTC_DEMANGLE_H

#ifdef __cplusplus
extern "C" {
#endif

// For size_t
#include <stddef.h>

char *rustc_demangle(const char *mangled, char *out, size_t *len, int *status);

#ifdef __cplusplus
}
#endif
#endif // RUSTC_DEMANGLE_H