/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef DFX_LOG_DEFINE_H
#define DFX_LOG_DEFINE_H

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxFaultLogger"

#define LOG_BUF_LEN 1024
#define FILENAME_ (__builtin_strrchr((__FILE__), '/') ? __builtin_strrchr((__FILE__), '/') + 1 : (__FILE__))

#ifdef __cplusplus
}
#endif
#endif
