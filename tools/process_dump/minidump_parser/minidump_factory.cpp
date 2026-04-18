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

#include "dfx_log.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"

namespace OHOS {
namespace HiviewDFX {

MinidumpStreamFactory& MinidumpStreamFactory::Instance()
{
    static MinidumpStreamFactory instance;
    return instance;
}

void MinidumpStreamFactory::RegisterCreator(uint32_t streamType, StreamCreator<MinidumpStream> creator)
{
    creators_[streamType] = creator;
}

std::shared_ptr<MinidumpStream> MinidumpStreamFactory::CreateStream(uint32_t streamType,
                                                                    std::shared_ptr<MinidumpMemoryReader> memoryReader)
{
    auto it = creators_.find(streamType);
    if (it != creators_.end()) {
        return it->second(memoryReader);
    }
    DFXLOGE("No creator registered for stream type %{public}u", streamType);
    return nullptr;
}

bool MinidumpStreamFactory::HasCreator(uint32_t streamType) const
{
    return creators_.find(streamType) != creators_.end();
}

void MinidumpStreamFactory::RegisterDefaultCreator()
{
    RegisterCreator(MinidumpException::streamType, MinidumpException::Instance);
    RegisterCreator(MinidumpEsrInfo::streamType, MinidumpEsrInfo::Instance);
    RegisterCreator(MinidumpMapList::streamType, MinidumpMapList::Instance);
    RegisterCreator(MinidumpMemoryList::streamType, MinidumpMemoryList::Instance);
    RegisterCreator(MinidumpMiscInfo::streamType, MinidumpMiscInfo::Instance);
    RegisterCreator(MinidumpModuleList::streamType, MinidumpModuleList::Instance);
    RegisterCreator(MinidumpSystemInfo::streamType, MinidumpSystemInfo::Instance);
    RegisterCreator(MinidumpThreadNameList::streamType, MinidumpThreadNameList::Instance);
    RegisterCreator(MinidumpThreadList::streamType, MinidumpThreadList::Instance);
}
}
}