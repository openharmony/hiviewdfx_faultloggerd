/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef DFX_SIGNAL_HANDLER_H
#define DFX_SIGNAL_HANDLER_H

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function for collecting thread information during a crash
 *
 * This function is invoked when a thread crash occurs, allowing custom
 * thread-specific information to be written into the provided buffer.
 *
 * @param buf Buffer for writing thread information
 * @param len Available buffer length in bytes
 * @param ucontext Pointer to user-level context information
 */
typedef void(*ThreadInfoCallBack)(char* buf, size_t len, void* ucontext);

/**
 * @brief Registers a callback for collecting thread information
 *
 * @param func Callback function pointer. Pass NULL to reset to default behavior.
 */
void SetThreadInfoCallback(ThreadInfoCallBack func);

/**
 * @brief Callback type for retrieving thread stack identifier
 *
 * @return Unique stack identifier (TLS-based) for asynchronous stack tracking
 */
typedef uint64_t(*GetStackIdFunc)(void);

/**
 * @brief Registers a callback for retrieving stack identifier
 *
 * @param func Callback function pointer. Pass NULL to disable stack ID tracking.
 */
void DFX_SetAsyncStackCallback(GetStackIdFunc func);

/**
 * @brief Retrieves the application's running unique identifier
 *
 * @return Pointer to a null-terminated string representing the unique ID
 *         Valid until next call to DFX_SetAppRunningUniqueId()
 */
const char* DFX_GetAppRunningUniqueId(void);

/**
 * @brief Sets the application's running unique identifier
 *
 * @param appRunningUniqueId Pointer to a null-terminated string (max 63 bytes)
 * @param len Length of the identifier string (excluding null terminator)
 * @return 0 on success, -1 on failure (invalid parameters or memory allocation error)
 */
int DFX_SetAppRunningUniqueId(const char* appRunningUniqueId, size_t len);

/**
 * @brief Types of crash objects for diagnostic information
 */
enum CrashObjType : uint8_t {
    OBJ_STRING = 0,       // Null-terminated string (max 1024 bytes)
    OBJ_MEMORY_64B,       // 64-byte memory block
    OBJ_MEMORY_256B,      // 256-byte memory block
    OBJ_MEMORY_1024B,     // 1KB memory block
    OBJ_MEMORY_2048B,     // 2KB memory block
    OBJ_MEMORY_4096B,     // 4KB memory block
};

/**
 * @brief Attaches diagnostic information to the current crash context
 *
 * Recommended usage in C++: Prefer RAII-based UniqueCrashObj from
 * dfx_unique_crash_obj.h for automatic resource management.
 *
 * @param type Type of diagnostic data (see CrashObjType)
 * @param addr Pointer to the data buffer (must remain valid until crash)
 * @return Handle to the previously set crash object (0 if none)
 * @note The data pointed to by 'addr' must remain valid until crash occurs
 */
uintptr_t DFX_SetCrashObj(uint8_t type, uintptr_t addr);

/**
 * @brief Detaches diagnostic information from the current crash context
 *
 * @param crashObj Handle returned by DFX_SetCrashObj()
 */
void DFX_ResetCrashObj(uintptr_t crashObj);

/**
 * @brief Configuration options for crash log generation
 */
enum CrashLogConfigType : uint8_t {
    EXTEND_PRINT_PC_LR = 0,    // extern PC/LR registers in crash logs
    CUT_OFF_LOG_FILE,          // log file size limits
    SIMPLIFY_PRINT_MAPS,       // simplified process maps
};

/**
 * @brief Configures crash log generation behavior
 *
 * @param type Configuration attribute (see CrashLogConfigType)
 * @param value Configuration value (semantics depend on type)
 * @return 0 on success, -1 on error (check errno for details)
 * @warning Non-thread-safe and non-signal-safe. Call early in program initialization.
 */
int DFX_SetCrashLogConfig(uint8_t type, uint32_t value);

#ifdef __cplusplus
}
#endif
#endif