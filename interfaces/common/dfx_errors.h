/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef DFX_ERRORS_H
#define DFX_ERRORS_H

#include <cinttypes>
#include <stdint.h>

namespace OHOS {
namespace HiviewDFX {
enum DwarfErrorCode : uint8_t {
    DWARF_ERROR_NONE,                           // No error.
    DWARF_ERROR_MEMORY_INVALID,                 // out of memory
    DWARF_ERROR_ILLEGAL_VALUE,                  // bad value
    DWARF_ERROR_ILLEGAL_STATE,                  // bad state
    DWARF_ERROR_STACK_INDEX_NOT_VALID,
    DWARF_ERROR_NOT_IMPLEMENTED,
    DWARF_ERROR_TOO_MANY_ITERATIONS,
    DWARF_ERROR_CFA_NOT_DEFINED,
    DWARF_ERROR_UNSUPPORTED_VERSION,            // unsupported version
    DWARF_ERROR_NO_FDES,
    DWARF_ERROR_NOT_SUPPORT,
};

enum QutErrorCode : uint16_t {
    QUT_ERROR_NONE = 0,                 // No error.
    QUT_ERROR_UNWIND_INFO,              // Unable to use unwind information to unwind.
    QUT_ERROR_INVALID_MAP,              // Unwind in an invalid map.
    QUT_ERROR_MAX_FRAMES_EXCEEDED,      // The number of frames exceed the total allowed.
    QUT_ERROR_REPEATED_FRAME,           // The last frame has the same pc/sp as the next.
    QUT_ERROR_INVALID_ELF,              // Unwind in an invalid elf.
    QUT_ERROR_QUT_SECTION_INVALID,      // Quicken unwind table is invalid.
    QUT_ERROR_INVALID_QUT_INSTR,        // Met a invalid quicken instruction.
    QUT_ERROR_MAPS_IS_NULL,             // Could not get maps
    QUT_ERROR_CONTEXT_IS_NULL,          // Could not get context
    QUT_ERROR_REQUEST_QUT_FILE_FAILED,  // Request QUT file failed.
    QUT_ERROR_REQUEST_QUT_INMEM_FAILED, // Request QUT in memory failed.
    QUT_ERROR_READ_STACK_FAILED,        // Read memory from stack failed.
    QUT_ERROR_TABLE_INDEX_OVERFLOW,     //
};

enum QutFileError : uint16_t {
    QUT_FILE_NONE = 0,
    QUT_FILE_NOT_INIT,
    QUT_FILE_NOT_WARMEDUP,
    QUT_FILE_LOAD_REQUESTING,
    QUT_FILE_OPEN_FILE_FAILED,
    QUT_FILE_FILE_STATE_ERROR,
    QUT_FILE_FILE_TOO_SHORT,
    QUT_FILE_MMAP_FAILED,
    QUT_FILE_QUTVERSION_NOT_MATCH,
    QUT_FILE_ARCH_NOT_MATCH,
    QUT_FILE_BUILDID_NOT_MATCH,
    QUT_FILE_FILE_LENGTH_NOT_MATCH,
    QUT_FILE_INSERT_NEW_QUT_FAILED,
    QUT_FILE_TRY_INVOKE_REQUEST_GENERATE,
    QUT_FILE_LOAD_FAILED,
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
