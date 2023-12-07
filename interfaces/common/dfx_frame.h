/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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
#ifndef DFX_FRAME_H
#define DFX_FRAME_H

#include <cinttypes>
#include <memory>
#include <string>
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMap;

/**
 * @brief Native Frame struct
 * It serves as the public definition of the native stack frame.
 */
struct DfxFrame {
    /** whether is Js frame */
    bool isJsFrame {false};
    /** frame index */
    size_t index {0};
    /** program counter register value */
    uint64_t pc {0};
    /** relative program counter value */
    uint64_t relPc {0};
    /** stack pointer value */
    uint64_t sp {0};
    /** map offset */
    uint64_t mapOffset {0};
    /** function byte offset */
    uint64_t funcOffset {0};
    /** elf file name */
    std::string mapName {""};
    /** function name */
    std::string funcName {""};
    /** elf file build id */
    std::string buildId {""};
    /** map cache */
    std::shared_ptr<DfxMap> map;
#if defined(ENABLE_MIXSTACK)
    /** Js frame code line */
    int32_t line {0};
    /** Js frame code column */
    int32_t column {0};
#endif
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
