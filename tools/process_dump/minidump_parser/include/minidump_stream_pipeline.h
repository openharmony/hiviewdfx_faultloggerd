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

#ifndef MINIDUMP_STREAM_PIPELINE_H
#define MINIDUMP_STREAM_PIPELINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "minidump_format.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {

class MinidumpParser;

class StreamPipeline {
public:
    explicit StreamPipeline(MinidumpParser& parser);
    ~StreamPipeline() = default;

    StreamPipeline(const StreamPipeline&) = delete;
    StreamPipeline& operator=(const StreamPipeline&) = delete;

    void ParseParallel(const std::vector<uint32_t>& streamTypes);

private:
    std::shared_ptr<MinidumpStream> ParseStream(uint32_t streamType);

    MinidumpParser& parser_;
    std::string path_;
};

}
}

#endif
