/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef MINIDUMP_FACTORY_H
#define MINIDUMP_FACTORY_H

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "minidump_error.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
template<typename T>
using StreamCreator = std::function<std::shared_ptr<T>(std::shared_ptr<MinidumpMemoryReader>)>;

class MinidumpStreamFactory {
public:
    static MinidumpStreamFactory& Instance();
    void RegisterCreator(uint32_t streamType, StreamCreator<MinidumpStream> creator);
    std::shared_ptr<MinidumpStream> CreateStream(uint32_t streamType,
                                                  std::shared_ptr<MinidumpMemoryReader> memoryReader);
    bool HasCreator(uint32_t streamType) const;

    MinidumpStreamFactory(const MinidumpStreamFactory&) = delete;
    MinidumpStreamFactory& operator=(const MinidumpStreamFactory&) = delete;

private:
    MinidumpStreamFactory() { RegisterDefaultCreator(); }
    void RegisterDefaultCreator();
    std::unordered_map<uint32_t, StreamCreator<MinidumpStream>> creators_;
};
}
}
#endif