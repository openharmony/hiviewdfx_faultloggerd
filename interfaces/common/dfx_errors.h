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
/**
 * @brief Dwarf unwind error code
 */
enum DwarfErrorCode : uint8_t {
    /** No error */
    DWARF_ERROR_NONE,
    /** Memory invalid */
    DWARF_ERROR_MEMORY_INVALID,
    /** Illegal value */
    DWARF_ERROR_ILLEGAL_VALUE,
    /** Illegal state */
    DWARF_ERROR_ILLEGAL_STATE,
    /** Stack index invalid */
    DWARF_ERROR_STACK_INDEX_NOT_VALID,
    /** Not implemented */
    DWARF_ERROR_NOT_IMPLEMENTED,
    /** Too many iterations */
    DWARF_ERROR_TOO_MANY_ITERATIONS,
    /** Cfa not defined */
    DWARF_ERROR_CFA_NOT_DEFINED,
    /** Unsupported version */
    DWARF_ERROR_UNSUPPORTED_VERSION,
    /** No FDEs */
    DWARF_ERROR_NO_FDES,
    /** Not support */
    DWARF_ERROR_NOT_SUPPORT,
};

/**
 * @brief Quick unwind table error code
 */
enum QutErrorCode : uint16_t {
    /** No error */
    QUT_ERROR_NONE = 0,
    /** Unable to use unwind information to unwind */
    QUT_ERROR_UNWIND_INFO,
    /** Unwind in an invalid map */
    QUT_ERROR_INVALID_MAP,
    /** The number of frames exceed the total allowed */
    QUT_ERROR_MAX_FRAMES_EXCEEDED,
    /** The last frame has the same pc/sp as the next */
    QUT_ERROR_REPEATED_FRAME,
    /** Unwind in an invalid elf */
    QUT_ERROR_INVALID_ELF,
    /** Quicken unwind table is invalid */
    QUT_ERROR_QUT_SECTION_INVALID,
    /** Met a invalid quicken instruction */
    QUT_ERROR_INVALID_QUT_INSTR,
    /** Could not get maps */
    QUT_ERROR_MAPS_IS_NULL,
    /** Could not get context */
    QUT_ERROR_CONTEXT_IS_NULL,
    /** Request QUT file failed */
    QUT_ERROR_REQUEST_QUT_FILE_FAILED,
    /** Request QUT in memory failed */
    QUT_ERROR_REQUEST_QUT_INMEM_FAILED,
    /** Read memory from stack failed */
    QUT_ERROR_READ_STACK_FAILED,
    /** Index overflow */
    QUT_ERROR_TABLE_INDEX_OVERFLOW,
};

/**
 * @brief Quick unwind table file error code
 */
enum QutFileError : uint16_t {
    /** File not found */
    QUT_FILE_NONE = 0,
    /** File not init */
    QUT_FILE_NOT_INIT,
    /** File not warmed up */
    QUT_FILE_NOT_WARMEDUP,
    /** File load requesting */
    QUT_FILE_LOAD_REQUESTING,
    /** File open failed */
    QUT_FILE_OPEN_FILE_FAILED,
    /** File state error */
    QUT_FILE_FILE_STATE_ERROR,
    /** File too short */
    QUT_FILE_FILE_TOO_SHORT,
    /** File mmap failed */
    QUT_FILE_MMAP_FAILED,
    /** Version dismatched */
    QUT_FILE_QUTVERSION_NOT_MATCH,
    /** Archtecture not matched */
    QUT_FILE_ARCH_NOT_MATCH,
    /** File build id dismatched */
    QUT_FILE_BUILDID_NOT_MATCH,
    /** File length dismatched */
    QUT_FILE_FILE_LENGTH_NOT_MATCH,
    /** Insert new quick unwind table failed */
    QUT_FILE_INSERT_NEW_QUT_FAILED,
    /** Try invode request generete */
    QUT_FILE_TRY_INVOKE_REQUEST_GENERATE,
    /** File load failed */
    QUT_FILE_LOAD_FAILED,
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
