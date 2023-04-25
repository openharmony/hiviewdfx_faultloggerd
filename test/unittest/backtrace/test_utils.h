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

/* This files contains c crasher header modules. */

#ifndef DFX_TEST_UTILS_H
#define DFX_TEST_UTILS_H

#include <string>
#include <cinttypes>

namespace OHOS {
namespace HiviewDFX {
uint32_t GetSelfFdCount();
uint32_t GetSelfMapsCount();
uint64_t GetSelfMemoryCount();
bool CheckContent(const std::string& content, const std::string& keyContent, bool checkExist);
bool CheckLogFileExist(int32_t pid, std::string& fileName);
}
}
#endif // DFX_TEST_UTILS_H
