/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#ifndef FAULTLOGGERD_TEST_H
#define FAULTLOGGERD_TEST_H

#include <cstdint>

constexpr const char* const TEST_TEMP_FILE_PATH = "/data/test/faultloggerd/temp/";

/**
 * the function to clear the temp files.
 */
void ClearTempFiles();

/**
 * the function to count the temp files.
 *
 * @return the count of temp files.
 */
uint64_t CountTempFiles();

#endif