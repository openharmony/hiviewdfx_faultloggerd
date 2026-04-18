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

#ifndef MINIDUMP_STREAM_H
#define MINIDUMP_STREAM_H

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "minidump_error.h"
#include "minidump_memory_reader.h"
#include "minidump_format.h"
#include "minidump_observer.h"
#include "minidump_optimizer.h"

namespace OHOS {
namespace HiviewDFX {
using namespace MinidumpFormat;
class MinidumpStream {
public:
    explicit MinidumpStream(std::shared_ptr<MinidumpMemoryReader> memoryReader)
        : memoryReader_(memoryReader), isValid_(false) {};
    MinidumpStream(const MinidumpStream&) = delete;
    void operator=(const MinidumpStream&) = delete;
    virtual ~MinidumpStream() = default;
    virtual bool Read(uint32_t expectedSize) = 0;
    void SetMinidumpSubject(std::shared_ptr<MinidumpSubject> subject) { subject_ = subject; }
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    bool Valid() const { return isValid_; }
protected:
    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    bool isValid_ = false;
    MinidumpErrorInfo lastError_;
    std::shared_ptr<MinidumpSubject> subject_;
};

class MinidumpContext {
public:
    explicit MinidumpContext(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    MinidumpContext(const MinidumpContext&) = delete;
    void operator=(const MinidumpContext&) = delete;

    uint32_t GetContextCPU() const;
    uint32_t GetContextFlags() const { return contextFlags_; }
    const std::shared_ptr<MDRawContextARM64> GetContextARM64() const;
    bool GetProgramCounter(uint64_t& pc) const;
    bool GetStackPointer(uint64_t& sp) const;
    void Print() const;
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    bool Valid() const { return isValid_; }
    bool Read(uint32_t expectedSize);

protected:
    void SetContextFlags(uint32_t contextFlags) { contextFlags_ = contextFlags; }
    void SetContextARM64(std::shared_ptr<MDRawContextARM64> context)
    {
        arm64Context_ = std::move(context);
    }

private:

    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    std::shared_ptr<MDRawContextARM64> arm64Context_;
    uint32_t contextFlags_;
    bool isValid_;
    MinidumpErrorInfo lastError_;
};

class MinidumpException : public MinidumpStream {
public:
    explicit MinidumpException(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpException>(memoryReader);
    }
    MinidumpException(const MinidumpException&) = delete;
    void operator=(const MinidumpException&) = delete;

    const MDExceptionStream* Exception() const
    {
        return isValid_ ? &exception_ : nullptr;
    }
    bool GetThreadID(uint32_t& threadId) const;
    const MDException* ExceptionRecord() const
    {
        return isValid_ ? &(exception_.exceptionRecord) : nullptr;
    }
    std::shared_ptr<MinidumpContext> GetContext();

    void Print();
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_EXCEPTION;

private:
    MDExceptionStream exception_;
    std::shared_ptr<MinidumpContext> context_;
};

class MinidumpMemoryRegion {
public:
    explicit MinidumpMemoryRegion(std::shared_ptr<MinidumpMemoryReader> memoryReader);

    std::shared_ptr<std::vector<uint8_t>> GetMemory();
    uint64_t GetBase() const;
    uint32_t GetSize() const;
    void FreeMemory();

    bool GetMemoryAtAddress(uint64_t address, uint8_t& value) const;
    bool GetMemoryAtAddress(uint64_t address, uint16_t& value) const;
    bool GetMemoryAtAddress(uint64_t address, uint32_t& value) const;
    bool GetMemoryAtAddress(uint64_t address, uint64_t& value) const;

    void Print() const;
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    bool Valid() const { return isValid_; }
    void SetDescriptor(MDMemoryDescriptor descriptor);
    const MDMemoryDescriptor GetDescriptor() { return descriptor_; }

private:
    template<typename T> bool GetMemoryAtAddressInternal(uint64_t address, T& value) const;

    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    MDMemoryDescriptor descriptor_;
    std::shared_ptr<std::vector<uint8_t>> memory_;
    bool isValid_;
    MinidumpErrorInfo lastError_;
};

class MinidumpMemoryList : public MinidumpStream {
public:
    explicit MinidumpMemoryList(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpMemoryList>(memoryReader);
    }
    MinidumpMemoryList(const MinidumpMemoryList&) = delete;
    void operator=(const MinidumpMemoryList&) = delete;

    uint32_t RegionCount() const { return isValid_ ? regionCount_ : 0; }
    std::shared_ptr<MinidumpMemoryRegion> GetMemoryRegionAtIndex(uint32_t index);
    std::shared_ptr<MinidumpMemoryRegion> GetMemoryRegionForAddress(uint64_t address);
    const std::vector<std::shared_ptr<MinidumpMemoryRegion>>& GetMemoryRegions() { return regions_; }

    void Print();
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_MEMORY_LIST;

private:
    bool ReadRegionCountAndDescriptors(std::vector<MDMemoryDescriptor>& descriptors);
    bool BuildMemoryRegions(const std::vector<MDMemoryDescriptor>& descriptors);

    std::vector<MDMemoryDescriptor> descriptors_;
    std::vector<std::shared_ptr<MinidumpMemoryRegion>> regions_;
    uint32_t regionCount_;
};

class MinidumpMiscInfo : public MinidumpStream {
public:
    explicit MinidumpMiscInfo(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpMiscInfo>(memoryReader);
    }
    MinidumpMiscInfo(const MinidumpMiscInfo&) = delete;
    void operator=(const MinidumpMiscInfo&) = delete;

    bool ProcessId(uint32_t& processId);
    bool ProcessTimes(uint32_t& processCreateTime,
        uint32_t& processUserTime, uint32_t& processKernelTime);

    void Print();
    bool Valid() const { return isValid_; }
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_MISC_INFO;

private:
    MDRawMiscInfo miscInfo_;
};

class MinidumpModule {
public:
    explicit MinidumpModule(std::shared_ptr<MinidumpMemoryReader> memoryReader);

    const MDRawModule* module() const { return &module_; }

    uint64_t BaseAddress() const
    {
        return isValid_ ? module_.baseOfImage : static_cast<uint64_t>(-1);
    }
    uint64_t Size() const
    {
        return isValid_ ? module_.sizeOfImage : 0;
    }
    std::string CodeFile() const;
    std::string CodeIdentifier() const;
    std::string DebugFile() const;
    std::string DebugIdentifier() const;
    std::string Version() const;
    bool IsUnloaded() const { return false; }

    void Print();
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    bool Valid() const { return isValid_; }
    bool Read();
    bool ReadAuxiliaryData();

private:
    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    bool moduleValid_;
    MDRawModule module_;
    std::shared_ptr<std::string> name_;
    bool isValid_;
    MinidumpErrorInfo lastError_;
};

class MinidumpModuleList : public MinidumpStream {
public:
    explicit MinidumpModuleList(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpModuleList>(memoryReader);
    }
    MinidumpModuleList(const MinidumpModuleList&) = delete;
    void operator=(const MinidumpModuleList&) = delete;

    uint32_t ModuleCount() const
    {
        return isValid_ ? moduleCount_ : 0;
    }
    std::vector<std::shared_ptr<MinidumpModule>> GetModules()
    {
        return modules_;
    }
    std::shared_ptr<MinidumpModule> GetModuleForAddress(uint64_t address);
    std::shared_ptr<MinidumpModule> GetModuleAtIndex(uint32_t index) const;
    std::shared_ptr<MinidumpModule> GetMainModule() const;

    void Print();
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_MODULE_LIST;

private:
    bool ReadModuleRawData(uint32_t moduleCount);
    bool ReadModuleAuxiliaryDataAndIndex(uint32_t moduleCount);

    std::vector<std::shared_ptr<MinidumpModule>> modules_;
    uint32_t moduleCount_;
};

class MinidumpSystemInfo : public MinidumpStream {
public:
    explicit MinidumpSystemInfo(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpSystemInfo>(memoryReader);
    }
    MinidumpSystemInfo(const MinidumpSystemInfo&) = delete;
    void operator=(const MinidumpSystemInfo&) = delete;

    const MDRawSystemInfo* SystemInfo() const
    {
        return isValid_ ? &systemInfo_ : nullptr;
    }
    std::string GetOS();
    std::string GetCPU();

    void Print();
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_SYSTEM_INFO;

private:
    MDRawSystemInfo systemInfo_;
};

class MinidumpThreadName {
public:
    explicit MinidumpThreadName(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    ~MinidumpThreadName() = default;

    const MDRawThreadName* ThreadName() const
    {
        return isValid_ ? &threadName_ : nullptr;
    }

    bool GetThreadID(uint32_t& threadId) const;
    std::string GetThreadName() const;

    void Print();
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    bool Valid() const { return isValid_; }
    bool Read();
    bool ReadAuxiliaryData();

private:
    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    bool threadNameValid_;
    MDRawThreadName threadName_;
    std::shared_ptr<std::string> name_;
    bool isValid_;
    MinidumpErrorInfo lastError_;
};

class MinidumpThreadNameList : public MinidumpStream {
public:
    explicit MinidumpThreadNameList(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpThreadNameList>(memoryReader);
    }
    MinidumpThreadNameList(const MinidumpThreadNameList&) = delete;
    void operator=(const MinidumpThreadNameList&) = delete;

    uint32_t ThreadNameCount() const
    {
        return isValid_ ? threadNameCount_ : 0;
    }

    std::shared_ptr<MinidumpThreadName> GetThreadNameAtIndex(uint32_t index) const;
    std::string GetThreadNameById(uint32_t threadId) const;
    const std::vector<std::shared_ptr<MinidumpThreadName>>& GetThreadNames() { return threadNames_;}

    void Print();
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_THREAD_NAME_LIST;

private:
    std::map<uint32_t, std::string> threadIdToNameMap_;
    std::vector<std::shared_ptr<MinidumpThreadName>> threadNames_;
    uint32_t threadNameCount_;
};

class MinidumpThread {
public:
    explicit MinidumpThread(std::shared_ptr<MinidumpMemoryReader> memoryReader);

    const MDRawThread& thread() const { return thread_; }
    std::shared_ptr<MinidumpMemoryRegion> GetMemory() const;
    std::shared_ptr<MinidumpContext> GetContext() const;
    bool GetThreadID(uint32_t& threadId) const;
    uint64_t GetStartOfStackMemoryRange() const;
    void Print();
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    bool Valid() const { return isValid_; }
    bool Read();
private:
    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    MDRawThread thread_;
    std::shared_ptr<MinidumpMemoryRegion> memory_;
    mutable std::shared_ptr<MinidumpContext> context_;
    bool isValid_;
    mutable MinidumpErrorInfo lastError_;
};

class MinidumpThreadList : public MinidumpStream {
public:
    explicit MinidumpThreadList(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpThreadList>(memoryReader);
    }
    MinidumpThreadList(const MinidumpThreadList&) = delete;
    void operator=(const MinidumpThreadList&) = delete;

    uint32_t ThreadCount() const { return isValid_ ? threadCount_ : 0; }
    std::shared_ptr<MinidumpThread> GetThreadAtIndex(uint32_t index) const;
    std::shared_ptr<MinidumpThread> GetThreadByID(uint32_t threadId);
    std::shared_ptr<std::vector<MinidumpThread>> GetThreads() { return threads_; }

    void Print();
    bool Read(uint32_t expectedSize);

    static const uint32_t streamType = MD_STREAM_THREAD_LIST;

private:
    bool ReadThreadCount(uint32_t expectedSize, uint32_t& threadCount);
    bool ReadThreadData(uint32_t threadCount);

    typedef std::map<uint32_t, std::shared_ptr<MinidumpThread>> IDToThreadMap;
    IDToThreadMap idToThreadMap_;
    std::shared_ptr<std::vector<MinidumpThread>> threads_;
    uint32_t threadCount_;
};

class MinidumpMapList : public MinidumpStream {
public:
    explicit MinidumpMapList(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpMapList>(memoryReader);
    }
    MinidumpMapList(const MinidumpMapList&) = delete;
    void operator=(const MinidumpMapList&) = delete;
    uint32_t GetMapsLength() const { return isValid_ ? mapsLength_ : 0; }
    std::vector<char> GetContents() const { return isValid_ ? contents_ : std::vector<char>(); }
    void Print();
    bool Read(uint32_t expectedSize);
    static const uint32_t streamType = MD_STREAM_LINUX_MAPS;
private:
  std::vector<char> contents_;
  uint32_t mapsLength_;
};

class MinidumpEsrInfo : public MinidumpStream {
public:
    explicit MinidumpEsrInfo(std::shared_ptr<MinidumpMemoryReader> memoryReader);
    static std::shared_ptr<MinidumpStream> Instance(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    {
        return std::make_shared<MinidumpEsrInfo>(memoryReader);
    }
    MinidumpEsrInfo(const MinidumpEsrInfo&) = delete;
    void operator=(const MinidumpEsrInfo&) = delete;

    const MDRawEsrRegsInfo& EsrRegsInfo() const { return esrRegsInfo_; }

    void Print();
    bool Read(uint32_t expectedSize);
    static const uint32_t streamType = MD_STREAM_ESR_INFO;
private:
    MDRawEsrRegsInfo esrRegsInfo_;
};
}
}
#endif