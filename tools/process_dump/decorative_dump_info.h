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

#ifndef DECORATIVE_DUMP_INFO_H
#define DECORATIVE_DUMP_INFO_H
#include <cinttypes>
#include <memory>
#include <set>
#include <vector>
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "dump_info.h"
#include "dump_info_factory.h"
#include "unwinder.h"
namespace OHOS {
namespace HiviewDFX {
#if defined(__arm__)
const uint64_t STEP = 4;
#define PRINT_FORMAT "%08x"
#elif defined(__aarch64__)
const uint64_t STEP = 8;
#define PRINT_FORMAT "%016llx"
#else
const uint64_t STEP = 8;
#define PRINT_FORMAT "%016llx"
#endif
class DecorativeDumpInfo : public DumpInfo {
public:
    void SetDumpInfo(const std::shared_ptr<DumpInfo>& dumpInfo) override
    {
        dumpInfo_ = dumpInfo;
    }

    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override
    {
        if (dumpInfo_ != nullptr) {
            dumpInfo_->Print(process, request, unwinder);
        }
    }

    int UnwindStack(DfxProcess& process, Unwinder& unwinder) override
    {
        if (dumpInfo_ != nullptr) {
            return dumpInfo_->UnwindStack(process, unwinder);
        }
        return 0;
    }

    void Symbolize(DfxProcess& process, Unwinder& unwinder) override
    {
        if (dumpInfo_ != nullptr) {
            dumpInfo_->Symbolize(process, unwinder);
        }
    }
protected:
    std::shared_ptr<DumpInfo> dumpInfo_ = nullptr;
};

class DumpInfoHeader : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<DumpInfoHeader>(); }
private:
    std::string GetReasonInfo(const ProcessDumpRequest& request, DfxProcess& process, DfxMaps& maps);
    CrashLogConfig GetCrashLogConfig(const ProcessDumpRequest& request);
    std::string GetLastFatalMsg(DfxProcess& process, const ProcessDumpRequest& request);
    std::string UpdateFatalMessageWhenDebugSignal(DfxProcess& process, const ProcessDumpRequest& request);
    std::string ReadCrashObjString(const ProcessDumpRequest& request) const;
};

class SubmitterStack : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<SubmitterStack>(); }
private:
    void GetSubmitterStack(pid_t tid, uint64_t stackId, Unwinder& unwinder,
        std::vector<DfxFrame>& submitterFrames);
};

class Registers : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<Registers>(); }
};

class ExtraCrashInfo : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<ExtraCrashInfo>(); }
private:
    std::string GetCrashObjContent(DfxProcess& process, const ProcessDumpRequest& request);
    std::string ReadCrashObjMemory(pid_t tid, uintptr_t addr, size_t length) const;
};

class OtherThreadDumpInfo : public DecorativeDumpInfo {
public:
    int UnwindStack(DfxProcess& process, Unwinder& unwinder) override;
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    void Symbolize(DfxProcess& process, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<OtherThreadDumpInfo>(); }
};

struct MemoryBlockInfo {
    uintptr_t startAddr;
    uintptr_t nameAddr;
    uintptr_t size;
    std::vector<uintptr_t> content;
    std::string name;
};

class MemoryNearRegister : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<MemoryNearRegister>(); }
private:
    void CollectRegistersBlock(pid_t tid, std::shared_ptr<DfxRegs> regs,
        std::shared_ptr<DfxMaps> maps, bool extendPcLrPrinting);
    void CreateMemoryBlock(pid_t tid, MemoryBlockInfo& blockInfo) const;
    std::vector<MemoryBlockInfo> registerBlocks_;
};

class FaultStack : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    void CollectStackInfo(pid_t tid, const std::vector<DfxFrame>& frames, bool needParseStack = false);
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<FaultStack>(); }
    const std::vector<uintptr_t>& GetStackValues();
private:
    bool CreateBlockForCorruptedStack(pid_t tid, const std::vector<DfxFrame>& frames,
                                      uintptr_t prevEndAddr, uintptr_t size);
    uintptr_t AdjustAndCreateMemoryBlock(pid_t tid, size_t index, uintptr_t prevSp,
                                         uintptr_t prevEndAddr, uintptr_t size);
    uintptr_t PrintMemoryBlock(const MemoryBlockInfo& info, uintptr_t stackStartAddr) const;
    void CreateMemoryBlock(pid_t tid, MemoryBlockInfo& blockInfo) const;
    std::vector<MemoryBlockInfo> blocks_;
    std::vector<uintptr_t> stackValues_;
};

class Maps : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<Maps>(); }
private:
    void SimplifyVma(DfxProcess& process, const std::shared_ptr<DfxMaps>& maps, std::set<DfxMap>& mapSet);
};

struct FDInfo {
    std::string path;
    uint64_t fdsanOwner;
};
using OpenFilesList = std::map<int, FDInfo>;
class OpenFiles : public DecorativeDumpInfo {
public:
    void Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder) override;
    static std::shared_ptr<DumpInfo> CreateInstance() { return std::make_shared<OpenFiles>(); }
private:
    bool ReadLink(std::string &src, std::string &dst);
    void CollectOpenFiles(OpenFilesList &list, pid_t pid);
    void FillFdsaninfo(OpenFilesList &list, pid_t nsPid, uint64_t fdTableAddr);
    std::string DumpOpenFiles(OpenFilesList files);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
