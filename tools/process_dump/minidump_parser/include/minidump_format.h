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

#ifndef MINIDUMP_FORMAT_H
#define MINIDUMP_FORMAT_H

#include <cstdint>

namespace OHOS {
namespace HiviewDFX {
namespace MinidumpFormat {

// ==================== Magic & Version ====================
constexpr uint32_t MINIDUMP_HEADER_SIGNATURE = 0x504d444d;  /* 'MDMP' little-endian */
constexpr uint32_t MINIDUMP_HEADER_SIGNATURE_SWAP = 0x4d444d50;  /* 'PMDM' (byte-swapped) */
constexpr uint32_t MINIDUMP_HEADER_VERSION = 42899;        // 0xa793
constexpr uint32_t MINIDUMP_OS_LINUX = 0x8201;
constexpr uint32_t MINIDUMP_OS_HONGMENG = 0x8209;

#define MD_CONTEXT_CPU_MASK 0xffffff00
#define MD_CONTEXT_ARM64 0x00400000

// ==================== Basic Types ====================
typedef uint32_t RVA;

// ==================== Core Structures ====================
struct __attribute__((packed, aligned(4))) MDRawHeader {
    uint32_t signature;
    uint32_t version;
    uint32_t numberOfStreams;
    RVA      streamDirectoryRva;
    uint32_t checksum;
    uint32_t timeStamp;
    uint64_t flags;
};

struct __attribute__((packed, aligned(4))) MDLocationDescriptor {
    uint32_t dataSize;
    RVA      rva;
};

struct __attribute__((packed, aligned(4))) MDMemoryDescriptor {
    uint64_t startOfMemoryRange;
    MDLocationDescriptor memory;
};

struct __attribute__((packed, aligned(4))) MDRawDirectory {
    uint32_t streamType;
    MDLocationDescriptor location;
};

struct __attribute__((packed, aligned(4))) MDString {
    uint32_t length;
    uint16_t buffer[1]; // UTF-16-encoded, 0-terminated
};

// ==================== Stream Type Enum ====================
enum MDStreamType : uint32_t {
    MD_STREAM_UNUSED               = 0,
    MD_STREAM_THREAD_LIST          = 3,
    MD_STREAM_MODULE_LIST          = 4,
    MD_STREAM_MEMORY_LIST          = 5,
    MD_STREAM_EXCEPTION            = 6,
    MD_STREAM_SYSTEM_INFO          = 7,
    MD_STREAM_HANDLE_DATA          = 12,
    MD_STREAM_UNLOADED_MODULE_LIST = 14,
    MD_STREAM_MISC_INFO            = 15,
    MD_STREAM_MEMORY_INFO_LIST     = 16,
    MD_STREAM_THREAD_INFO_LIST     = 17,
    MD_STREAM_THREAD_NAME_LIST     = 24,
    MD_STREAM_ESR_INFO             = 0x47670001,  /* MDRawEsrInfo  */
    MD_STREAM_LINUX_MAPS           = 0x47670009,  /* /proc/$x/maps      */
    MD_STREAM_LAST_RESERVED        = 0x0000ffff,
};

// ==================== Misc Info Flags1 Enum ====================
enum MDMiscInfoFlags1 : uint32_t {
    MD_MISCINFO_FLAGS1_PROCESS_ID            = 0x00000001,
    MD_MISCINFO_FLAGS1_PROCESS_TIMES         = 0x00000002,
    MD_MISCINFO_FLAGS1_PROCESSOR_POWER_INFO  = 0x00000004,
    MD_MISCINFO_FLAGS1_PROCESS_INTEGRITY     = 0x00000010,
    MD_MISCINFO_FLAGS1_PROCESS_EXECUTE_FLAGS = 0x00000020,
    MD_MISCINFO_FLAGS1_TIMEZONE              = 0x00000040,
    MD_MISCINFO_FLAGS1_PROTECTED_PROCESS     = 0x00000080,
    MD_MISCINFO_FLAGS1_BUILDSTRING           = 0x00000100,
    MD_MISCINFO_FLAGS1_PROCESS_COOKIE        = 0x00000200,
};

// ==================== CPU Architecture Enum ====================
enum MDCpuArchitecture : uint16_t {
    MD_CPU_ARCH_ARM64   = 12,
    MD_CPU_ARCH_ARM64_OLD = 0x8003,
    MD_CPU_ARCH_UNKNOWN = 0xffff,
};

// ==================== Stream Structures ====================
struct __attribute__((packed, aligned(4))) MDRawThread {
    uint32_t threadId;
    uint32_t suspendCount;
    uint32_t priorityClass;
    uint32_t priority;
    uint64_t teb;
    MDMemoryDescriptor stack;
    MDLocationDescriptor threadContext;
};

struct __attribute__((packed, aligned(4))) MDRawThreadList {
    uint32_t numberOfThreads;
    MDRawThread threads[1];
};

struct __attribute__((packed, aligned(4))) MDRawThreadName {
    uint32_t threadId;
    uint64_t rvaOfThreadName;
};

struct __attribute__((packed, aligned(4))) MDRawThreadNameList {
    uint32_t numberOfThreadNames;
    MDRawThreadName threadNames[1];
};

struct __attribute__((packed, aligned(4))) MDVSFixedFileInfo {
    uint32_t signature;
    uint32_t structVersion;
    uint32_t fileVersionHi;
    uint32_t fileVersionLo;
    uint32_t productVersionHi;
    uint32_t productVersionLo;
    uint32_t fileFlagsMask;
    uint32_t fileFlags;
    uint32_t fileOs;
    uint32_t fileType;
    uint32_t fileSubtype;
    uint32_t fileDateHi;
    uint32_t fileDateLo;
};

struct __attribute__((packed, aligned(4))) MDRawModule {
    uint64_t baseOfImage;
    uint32_t sizeOfImage;
    uint32_t checksum;
    uint32_t timeDataStamp;
    RVA      moduleNameRva;
    MDVSFixedFileInfo versionInfo;
    MDLocationDescriptor cvRecord;
    MDLocationDescriptor miscRecord;
    uint32_t reserved0[2];
    uint32_t reserved1[2];
};

struct __attribute__((packed, aligned(4))) MDRawModuleList {
    uint32_t numberOfModules;
    MDRawModule modules[1];
};

struct __attribute__((packed, aligned(4))) MDRawMemoryList {
    uint32_t numberOfMemoryRanges;
    MDMemoryDescriptor memoryRanges[1];
};

#define MD_EXCEPTION_MAXIMUM_PARAMETERS 15u
struct __attribute__((packed, aligned(4))) MDException {
    uint32_t exceptionCode;
    uint32_t exceptionFlags;
    uint64_t exceptionRecord;
    uint64_t exceptionAddress;
    uint32_t numberParameters;
    uint32_t unusedAlignment;
    uint64_t exceptionInformation[MD_EXCEPTION_MAXIMUM_PARAMETERS];
};

struct __attribute__((packed, aligned(4))) MDExceptionStream {
    uint32_t threadId;
    uint32_t unusedAlignment;
    MDException exceptionRecord;
    MDLocationDescriptor threadContext;
};

constexpr uint32_t MD_FLOATINGSAVEAREA_ARM64_FPR_COUNT = 32;
constexpr uint32_t MD_CONTEXT_ARM64_GPR_COUNT = 33;

// ==================== Arm64 context Structures ====================
struct __attribute__((packed, aligned(4))) uint128_struct {
    uint64_t lo;
    uint64_t hi;
};
struct __attribute__((packed, aligned(4))) MDRawContextARM64 {
    uint32_t contextFlags;
    uint32_t cpsr;

    uint64_t iregs[MD_CONTEXT_ARM64_GPR_COUNT];     // x0-x32

    uint128_struct fpregs[MD_FLOATINGSAVEAREA_ARM64_FPR_COUNT];   // NEON registers v0-v31
    uint32_t fpcr;  // Floating-point Control Register
    uint32_t fpsr;  // Floating-point status register

    uint64_t bcr[8];  // Breakpoint Control Register
    uint64_t bvr[8];  // Breakpoint Value Register
    uint64_t wcr[2];  // Observation Control Register
    uint64_t wvr[2];  // Observation Value Register
};

struct __attribute__((packed, aligned(4))) MDRawSystemInfo {
    uint16_t processorArchitecture;
    uint16_t processorLevel;
    uint16_t processorRevision;
    uint8_t  numberOfProcessors;
    uint8_t  productType;
    uint32_t majorVersion;
    uint32_t minorVersion;
    uint32_t buildNumber;
    uint32_t platformId;
    RVA      csdVersionRva;
    uint16_t suiteMask;
    uint16_t reserved2;
    uint32_t cpuInfo[2];
};

struct __attribute__((packed, aligned(4))) MDRawUnloadedModule {
    uint64_t baseOfImage;
    uint32_t sizeOfImage;
    uint32_t checksum;
    uint32_t timeDataStamp;
    RVA rvaOfmoduleName;
};

struct __attribute__((packed, aligned(4))) MDRawUnloadedModuleList {
    uint32_t sizeOfHeader;
    uint32_t sizeOfEntry;
    uint32_t numberOfEntries;
};

struct __attribute__((packed, aligned(4))) MDRawMiscInfo {
    uint32_t sizeOfInfo;
    uint32_t flags1;
    uint32_t processId;
    uint32_t processCreateTime;
    uint32_t processUserTime;
    uint32_t processKernelTime;
    uint32_t processorMaxMhz;
    uint32_t processorCurrentMhz;
    uint32_t processorMhzLimit;
    uint32_t processorMaxIdleState;
    uint32_t processorCurrentIdleState;
    uint32_t processIntegrityLevel;  // Process integrity level
    uint32_t processExecuteFlags;    // Process execution flag
    uint32_t protectedProcess;       // Protected process
};

struct __attribute__((packed, aligned(4))) MDRawMemoryInfo {
    uint64_t baseAddress;
    uint64_t allocationBase;
    uint32_t allocationProtect;
    uint32_t reserved1;
    uint64_t regionSize;
    uint32_t state;
    uint32_t protection;
    uint32_t type;
    uint32_t reserved2;
};

struct __attribute__((packed, aligned(4))) MDRawMemoryInfoList {
    uint32_t sizeOfHeader;
    uint32_t sizeOfEntry;
    uint64_t numberOfEntries;
};

struct __attribute__((packed, aligned(4))) MDRawEsrRegsInfo {
    uint32_t validity;
    uint32_t dumpThreadId;
    uint64_t esrRegs;
};
 	 
enum MDBreakpadInfoValidity : uint32_t {
    MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID     = 1 << 0,
    MD_BREAKPAD_INFO_VALID_ESR_REGS           = 1 << 1
};
} // namespace MinidumpFormat
} // namespace HiviewDFX
} // namespace OHOS

#endif // MINIDUMP_FORMAT_H
