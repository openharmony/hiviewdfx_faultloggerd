/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef DFX_UNIQUE_CRASH_OBJ
#define DFX_UNIQUE_CRASH_OBJ

#include "dfx_signal_handler.h"

namespace OHOS {
namespace HiviewDFX {

/**
 * @brief RAII wrapper for managing crash diagnostic objects
 *
 * Automatically attaches and detaches diagnostic information to the crash context
 * using the Resource Acquisition Is Initialization (RAII) pattern. This ensures
 * that diagnostic data is properly registered on construction and unregistered
 * on destruction, even if exceptions occur.
 *
 * @warning The diagnostic data buffer pointed to by the constructor argument
 *          must remain valid for the lifetime of this object.
 *
 * Example Usage:
 * @code
 * void ProcessRequest(const std::string& requestId)
 * {
 *     // Attach request ID to crash context for diagnostic purposes
 *     UniqueCrashObj crashInfo(OBJ_STRING, requestId.c_str());
 *     // ... perform operations that might crash ...
 * } // crashInfo is automatically detached here
 * @endcode
 *
 * @dependency Add the following to your BUILD.gn:
 * @code
 * external_deps += [ "faultloggerd:dfx_signalhandler" ]
 * @endcode
 */
class UniqueCrashObj final {
public:
    /**
     * @brief Constructs a UniqueCrashObj and attaches diagnostic data
     *
     * @param objType Type of diagnostic data (see CrashObjType enum)
     * @param objAddr Pointer to the data buffer. Must remain valid until destruction.
     */
    explicit UniqueCrashObj(CrashObjType objType, uintptr_t objAddr)
        : lastObjAddr_(DFX_SetCrashObj(objType, objAddr)) {}

    /**
     * @brief Destructor automatically detaches diagnostic data
     */
    ~UniqueCrashObj()
    {
        DFX_ResetCrashObj(lastObjAddr_);
    }

    // Disable copy operations to prevent double detachment
    UniqueCrashObj(const UniqueCrashObj&) = delete;
    UniqueCrashObj& operator=(const UniqueCrashObj&) = delete;

private:
    uintptr_t lastObjAddr_; // handle to the previously set crash object
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // DFX_UNIQUE_CRASH_OBJ