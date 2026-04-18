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

#pragma once

#include <cstring>
#include <securec.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "minidump_config.h"
#include "minidump_error.h"
#include "minidump_factory.h"
#include "minidump_format.h"
#include "minidump_memory_reader.h"
#include "minidump_observer.h"
#include "minidump_optimizer.h"
#include "minidump_parser.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {

inline std::shared_ptr<std::istream> MakeStream(const std::string& data)
{
    auto ss = std::make_shared<std::stringstream>(data, std::ios::binary | std::ios::in);
    return ss;
}

inline std::shared_ptr<MinidumpMemoryReader> MakeReader(const std::string& data)
{
    return std::make_shared<MinidumpMemoryReader>(MakeStream(data));
}
} // namespace HiviewDFX
} // namespace OHOS
