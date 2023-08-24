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

namespace OHOS {
namespace HiviewDFX {
/**
 * @brief Unwind error data
 */
struct UnwindErrorData {
    uint16_t code;
    uint64_t addr;
};

/**
 * @brief Unwind error code
 */
enum UnwindErrorCode : uint16_t {
    /** No error */
    UNW_ERROR_NONE = 0,
    /** Invalid unwind mode */
    UNW_ERROR_INVALID_MODE,
    /** Invalid unwind context */
    UNW_ERROR_INVALID_CONTEXT,
    /** Invalid unwind regs */
    UNW_ERROR_INVALID_REGS,
    /** Invalid unwind map */
    UNW_ERROR_INVALID_MAP,
    /** Invalid unwind elf */
    UNW_ERROR_INVALID_ELF,
    /** Invalid unwind pid */
    UNW_ERROR_INVALID_PID,
    /** Invalid unwind stack index */
    UNW_ERROR_INVALID_STACK_INDEX,
    /** Reserved value */
    UNW_ERROR_RESERVED_VALUE,
    /** Illegal value */
    UNW_ERROR_ILLEGAL_VALUE,
    /** Illegal state */
    UNW_ERROR_ILLEGAL_STATE,
    /** The last frame has the same pc/sp as the next frame */
    UNW_ERROR_REPEATED_FRAME,
    /** The number of frames exceed the total allowed */
    UNW_ERROR_MAX_FRAMES_EXCEEDED,
    /** arm exidx invalid alignment */
    UNW_ERROR_INVALID_ALIGNMENT,
    /** arm exidx invalid personality */
    UNW_ERROR_INVALID_PERSONALITY,
    /** No unwind info */
    UNW_ERROR_NO_UNWIND_INFO,
    /** arm exidx cant unwind */
    UNW_ERROR_CANT_UNWIND,
    /** arm exidx spare */
    UNW_ERROR_ARM_EXIDX_SPARE,
    /** arm exidx finish */
    UNW_ERROR_ARM_EXIDX_FINISH,
    /** Quicken unwind section is invalid */
    UNW_ERROR_INVALID_QUT_SECTION,
    /** Met a invalid quicken instruction */
    UNW_ERROR_INVALID_QUT_INSTRUCTION,
    /** Request QUT file failed */
    UNW_ERROR_REQUEST_QUT_FILE_FAILED,
    /** Request QUT in memory failed */
    UNW_ERROR_REQUEST_QUT_INMEM_FAILED,
    /** Read memory from stack failed */
    UNW_ERROR_READ_STACK_FAILED,
    /** Index overflow */
    UNW_ERROR_TABLE_INDEX_OVERFLOW,
    /** Too many iterations */
    UNW_ERROR_TOO_MANY_ITERATIONS,
    /** Cfa not defined */
    UNW_ERROR_DWARF_CFA_NOT_DEFINED,
    /** No FDEs */
    UNW_ERROR_DWARF_NO_FDES,
    /** Unsupported version */
    UNW_ERROR_UNSUPPORTED_VERSION,
    /** Not support */
    UNW_ERROR_NOT_SUPPORT,
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
