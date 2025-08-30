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

#ifndef KERNEL_SNAPSHOT_PROCESSOR_IMPL_H
#define KERNEL_SNAPSHOT_PROCESSOR_IMPL_H

#include "i_kernel_snapshot_processor.h"

#include "kernel_snapshot_parser.h"
#include "kernel_snapshot_printer.h"
#include "kernel_snapshot_reporter.h"

namespace OHOS {
namespace HiviewDFX {
class KernelSnapshotProcessorImpl : public IKernelSnapshotProcessor {
public:
    KernelSnapshotProcessorImpl() : parser_(std::unique_ptr<IKernelSnapshotParser>(new KernelSnapshotParser())),
        printer_(std::unique_ptr<IKernelSnapshotPrinter>(new KernelSnapshotPrinter())),
        reporter_(std::unique_ptr<IKernelSnapshotReporter>(new KernelSnapshotReporter())) {}

    void Process(const std::string& snapshot) override;
    KernelSnapshotProcessorImpl(KernelSnapshotProcessorImpl&) = delete;
    KernelSnapshotProcessorImpl& operator=(KernelSnapshotProcessorImpl&) = delete;
private:
    std::unique_ptr<IKernelSnapshotParser> parser_;
    std::unique_ptr<IKernelSnapshotPrinter> printer_;
    std::unique_ptr<IKernelSnapshotReporter> reporter_;
};
} // namespace HiviewDFX
} // namespace OHOS

#ifdef __cplusplus
extern "C" {
#endif
void ProcessKernelSnapShot(const std::string& kernelSnapShotMsg);
#ifdef __cplusplus
}
#endif
#endif
