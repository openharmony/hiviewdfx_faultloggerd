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

#include "minidump_stream_pipeline.h"

#include <chrono>
#include <future>
#include <memory>

#include "dfx_log.h"
#include "minidump_config.h"
#include "minidump_factory.h"
#include "minidump_index.h"
#include "minidump_memory_reader.h"
#include "minidump_optimizer.h"
#include "minidump_parser.h"

namespace OHOS {
namespace HiviewDFX {

StreamPipeline::StreamPipeline(MinidumpParser& parser)
    : parser_(parser), path_(parser.Path())
{
}

std::shared_ptr<MinidumpStream> StreamPipeline::ParseStream(uint32_t streamType)
{
    const MDRawDirectory* dirEntry = parser_.GetDirectoryEntryAtType(streamType);
    if (dirEntry == nullptr) {
        return nullptr;
    }

    auto reader = std::make_shared<MinidumpMemoryReader>(path_);
    if (!reader->ValidateStreamExtent(dirEntry->location.rva, dirEntry->location.dataSize)) {
        DFXLOGE("StreamPipeline: stream %{public}u extent out of bounds", streamType);
        return nullptr;
    }
    if (!reader->SeekSet(dirEntry->location.rva)) {
        DFXLOGE("StreamPipeline: cannot seek to stream %{public}u", streamType);
        return nullptr;
    }

    auto stream = MinidumpStreamFactory::Instance().CreateStream(streamType, reader);
    if (stream == nullptr) {
        DFXLOGE("StreamPipeline: cannot create stream %{public}u", streamType);
        return nullptr;
    }

    stream->SetMinidumpSubject(parser_.GetSubject());
    stream->SetAddressIndexes(
        PerformanceOptimizer::Instance().GetMemoryAddressIndex(),
        PerformanceOptimizer::Instance().GetModuleAddressIndex());

    if (!stream->Read(dirEntry->location.dataSize)) {
        DFXLOGE("StreamPipeline: cannot read stream %{public}u", streamType);
        return nullptr;
    }

    return stream;
}

void StreamPipeline::ParseParallel(const std::vector<uint32_t>& streamTypes)
{
    if (path_.empty()) {
        DFXLOGW("StreamPipeline: no file path, skipping parallel parsing");
        return;
    }

    auto& configMgr = MinidumpConfigManager::Instance();
    auto config = configMgr.GetConfig();
    if (!config.enableParallelParsing) {
        DFXLOGI("StreamPipeline: parallel parsing disabled");
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    std::vector<std::future<std::shared_ptr<MinidumpStream>>> futures;
    futures.reserve(streamTypes.size());

    for (uint32_t streamType : streamTypes) {
        futures.push_back(std::async(std::launch::async, [this, streamType]() {
            return ParseStream(streamType);
        }));
    }

    for (size_t i = 0; i < futures.size(); ++i) {
        auto result = futures[i].get();
        if (result != nullptr) {
            parser_.InjectStream(streamTypes[i], std::move(result));
        } else {
            DFXLOGW("StreamPipeline: failed to parse stream %{public}u", streamTypes[i]);
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    DFXLOGI("StreamPipeline: parsed %{public}zu streams in %{public}lld ms",
            streamTypes.size(), static_cast<long long>(elapsed));
}

}
}
