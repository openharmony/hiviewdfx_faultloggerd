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
#include "decorative_dump_info.h"
#ifndef is_ohos_lite
#include "async_stack.h"
#endif
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "unwinder.h"
#ifndef is_ohos_lite
#include "unique_stack_table.h"
#endif
#include "dfx_util.h"
#include "dfx_buffer_writer.h"
namespace OHOS {
namespace HiviewDFX {
REGISTER_DUMP_INFO_CLASS(SubmitterStack);

void SubmitterStack::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    auto thread = process.GetKeyThread();
    std::vector<DfxFrame> submitterFrames;
    GetSubmitterStack(thread->GetThreadInfo().nsTid, request.stackId, unwinder, submitterFrames);
    if (submitterFrames.empty()) {
        return;
    }
    thread->SetSubmitterFrames(submitterFrames);
    std::string stackStr = "========SubmitterStacktrace========\n";
    stackStr += DumpUtils::GetStackTrace(submitterFrames);
    DfxBufferWriter::GetInstance().WriteMsg(stackStr);
    DfxBufferWriter::GetInstance().AppendBriefDumpInfo(stackStr);
}

void SubmitterStack::GetSubmitterStack(pid_t tid, uint64_t stackId, Unwinder& unwinder,
    std::vector<DfxFrame>& submitterFrames)
{
#ifndef is_ohos_lite
    if (stackId == 0) {
        DFXLOGW("stackId has not been initialized!");
        return;
    }
    const std::shared_ptr<DfxMaps>& maps = unwinder.GetMaps();
    if (maps == nullptr) {
        DFXLOGE("maps is nullptr!");
        return;
    }
    std::vector<std::shared_ptr<DfxMap>> mapVec;
    if (!maps->FindMapsByName("[anon:async_stack_table]", mapVec)) {
        DFXLOGE("can not find map of async stack table!");
        return;
    }
    auto map = mapVec.front();
    size_t size = map->end - map->begin;
    auto tableData = std::make_shared<std::vector<uint8_t>>(size);
    size_t byte = ReadProcMemByPid(tid, map->begin, tableData->data(), size);
    if (byte != size) {
        DFXLOGE("Failed to read unique_table from target");
        return;
    }
    UniqueStackTable stackTable(tableData->data(), size, false);
    std::vector<uintptr_t> pcs;
    StackId id;
    id.value = stackId;
    if (!stackTable.GetPcsByStackId(id, pcs)) {
        DFXLOGW("Failed to get pcs by stackId");
        return;
    }
    unwinder.EnableParseNativeSymbol(true);
    unwinder.GetFramesByPcs(submitterFrames, pcs);
    unwinder.EnableParseNativeSymbol(false);
#endif
}
}
}