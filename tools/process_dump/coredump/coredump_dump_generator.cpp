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
#include "coredump_dump_generator.h"

#include "coredump_controller.h"
#include "coredump_header_writer.h"
#include "coredump_mapping_manager.h"
#include "coredump_note_segment_writer.h"
#include "dump_utils.h"
#include "faultloggerd_client.h"

namespace OHOS {
namespace HiviewDFX {
bool CoredumpGenerator::TriggerCoredump()
{
    DFXLOGI("Begin to do coredump pid: %{public}d", request_.pid);
    StartCoredumpCb(request_.pid, getpid());
    LoadConfig();
    if (!MmapCoredumpFile()) {
        return false;
    }
    WriteSegmentHeader();
    WriteElfHeader();
    WriteNoteSegment();
    return true;
}

void CoredumpGenerator::DumpMemoryForPid(pid_t vmPid)
{
    if (!bw_) {
        return;
    }
    WriteLoadSegment(vmPid);
    WriteSectionHeader();
    auto ret = AdjustCoredumpFile();
    if (!ret) {
        fileMgr_.UnlinkFile(fileMgr_.GetCoredumpFilePath());
    }
    FinishCoredump(ret);
}

void CoredumpGenerator::FinishCoredump(bool ret)
{
    std::string fileName = ret ? fileMgr_.GetCoredumpFileName() : "";
    int32_t retCode = ret ? ResponseCode::REQUEST_SUCCESS : ResponseCode::CORE_DUMP_GENERATE_FAIL;

    DFXLOGI("%{public}s bundlerName %{public}s", __func__, fileName.c_str());
    FinishCoredumpCb(coredumpProc_.targetPid, fileName, retCode);

    if (CoredumpController::IsHwasanCoredumpEnabled()) {
        DumpUtils::InfoCrashUnwindResult(request_, true);
    }
    exit(0);
}

bool CoredumpGenerator::MmapCoredumpFile()
{
    fileMgr_.SetTargetPid(request_.pid);
    if (!fileMgr_.CreateFile() || !fileMgr_.MmapForFd()) {
        return false;
    }

    bw_ = std::make_unique<CoredumpBufferWriter>(fileMgr_.GetMappedMemory(), fileMgr_.GetCoreFileSize());
    return true;
}

bool CoredumpGenerator::AdjustCoredumpFile()
{
    DFXLOGI("Coredump end: pid = %{public}d, elapsed time = %{public}" PRId64 "ms",
        coredumpProc_.targetPid, counter_.Elapsed<std::chrono::milliseconds>());
    if (!bw_) {
        return false;
    }

    if (!fileMgr_.AdjustFileSize(bw_->GetOffset())) {
        return false;
    }
    return true;
}

bool CoredumpGenerator::WriteElfHeader()
{
    CoredumpElfHeaderWriter elfWriter(*bw_);
    elfWriter.InitElfHeader(ePhnum_);
    return elfWriter.Write();
}

bool CoredumpGenerator::WriteSegmentHeader()
{
    DFXLOGI("Begin to Write segment header");
    if (!bw_) {
        DFXLOGE("buffer writer not init");
        return false;
    }
    bw_->Advance(sizeof(Elf64_Ehdr));
    ProgramSegmentHeaderWriter programSegmentHeader(CoredumpMappingManager::GetInstance().GetMaps(), *bw_);
    programSegmentHeader.Write();
    ePhnum_ = programSegmentHeader.GetPhnum();
    return true;
}

bool CoredumpGenerator::WriteNoteSegment()
{
    if (!bw_) {
        return false;
    }
    DFXLOGI("Begin to Write note segment");
    if (!coredumpProc_.IsValid()) {
        DFXLOGE("targetPid or targetTid is 0!");
        return false;
    }

    NoteSegmentWriter note(coredumpProc_, CoredumpMappingManager::GetInstance().GetMaps(), *bw_);
    note.SetKeyThreadData(coredumpKeyThread_);
    note.Write();
    return true;
}

bool CoredumpGenerator::WriteLoadSegment(pid_t vmPid)
{
    DFXLOGI("Begin to Write load segment");
    if (vmPid == 0) {
        DFXLOGE("vmPid is 0!");
        return false;
    }
    if (!bw_) {
        return false;
    }
    LoadSegmentWriter segment(vmPid, ePhnum_, *bw_);
    segment.Write();
    return true;
}

bool CoredumpGenerator::WriteSectionHeader()
{
    DFXLOGI("Begin to Write section header");
    if (!bw_) {
        return false;
    }

    SectionHeaderTableWriter sectionHeaderTable(*bw_);
    sectionHeaderTable.Write();
    return true;
}

CoredumpGenerator::CoredumpGenerator(const ProcessDumpRequest& request) : request_(request)
{
    coredumpProc_.Init(request_.pid, request_.tid);
    coredumpKeyThread_.Init(request_.tid, DfxRegs::CreateFromUcontext(request_.context));
}

bool FullCoredumpGenerator::LoadConfig()
{
    auto& configManager = CoredumpConfigManager::GetInstance();
    return configManager.LoadCoredumpConfig("FULL");
}

bool HwAsanDumpGenerator::LoadConfig()
{
    auto& configManager = CoredumpConfigManager::GetInstance();
    return configManager.LoadCoredumpConfig("HWASAN");
}
}
}
