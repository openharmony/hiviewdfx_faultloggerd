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
    /** map item */
    std::shared_ptr<DfxMap> map;
};

struct CallFrame {
    DfxFrame frame;
    int32_t symbolFileIndex = -1; // symbols file index, used to report protobuf file
    uint64_t vaddrInFile = 0; // vaddr of symbol in file
    uint64_t offsetToVaddr = 0; // offset of ip to vaddr
    int32_t symbolIndex = -1; // symbols index , should update after sort

    // add for constructor function
    CallFrame(uint64_t pc, uint64_t sp)
    {
        frame.pc = pc;
        frame.sp = sp;
    }

    // add for sort function
    bool operator==(const CallFrame &b) const
    {
        return (frame.pc == b.frame.pc) && (frame.sp == b.frame.sp);
    }
    bool operator!=(const CallFrame &b) const
    {
        return (frame.pc != b.frame.pc) || (frame.sp != b.frame.sp);
    }

    std::string ToString() const
    {
        return StringPrintf("ip: 0x%016llx sp: 0x%016llx", frame.pc, frame.sp);
    }
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
