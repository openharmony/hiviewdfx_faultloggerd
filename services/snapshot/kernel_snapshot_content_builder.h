/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef KERNEL_SNAPSHOT_CONTENT_BUILDER_H
#define KERNEL_SNAPSHOT_CONTENT_BUILDER_H

#include "kernel_snapshot_data.h"

namespace OHOS {
namespace HiviewDFX {
class KernelSnapshotContentBuilder {
public:
    explicit KernelSnapshotContentBuilder(CrashMap& crashData, bool isLocal = false)
        : crashData_(crashData), isLocal_(isLocal) {}
    std::string GenerateSummary();

    KernelSnapshotContentBuilder(const KernelSnapshotContentBuilder&) = delete;
    KernelSnapshotContentBuilder& operator=(const KernelSnapshotContentBuilder&) = delete;

private:
    struct SnapshotUserSection {
        std::string title;
        std::string content;
        std::string suffix;

        SnapshotUserSection(std::string title, std::string content, std::string suffix)
            : title(std::move(title)), content(std::move(content)), suffix(std::move(suffix)) {}
        bool IsEmptyContent() const;
        size_t Length() const;
        void WriteTo(std::string& output) const;
    };

    std::vector<SnapshotUserSection> BuildSections();
    size_t ComputeTotalLength();
    std::string ComposeSummary();

    CrashMap& crashData_;
    bool isLocal_;
    std::vector<SnapshotUserSection> contentSections_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
