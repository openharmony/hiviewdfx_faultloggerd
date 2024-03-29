/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "dwarf_unwinder.h"

// dfx_log header must be included in front of libunwind header
#include "dfx_log.h"

#include <securec.h>
#include <libunwind.h>
#include <libunwind_i-ohos.h>

#include "dfx_define.h"
#include "dfx_config.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfUnwinder"
constexpr int32_t MIN_VALID_FRAME_COUNT = 3;
}

DwarfUnwinder::DwarfUnwinder()
{
    frames_.clear();
}

DwarfUnwinder::~DwarfUnwinder()
{
    frames_.clear();
}

const std::vector<DfxFrame>& DwarfUnwinder::GetFrames() const
{
    return frames_;
}

void DwarfUnwinder::UpdateFrameFuncName(unw_addr_space_t as,
    std::shared_ptr<DfxSymbols> symbol, DfxFrame& frame)
{
    if (symbol != nullptr) {
        symbol->GetNameAndOffsetByPc(as, frame.pc, frame.funcName, frame.funcOffset);
    }
}

bool DwarfUnwinder::Unwind(size_t skipFrameNum, size_t maxFrameNums)
{
    unw_addr_space_t as;
    unw_init_local_address_space(&as);
    if (as == nullptr) {
        return false;
    }
    auto symbol = std::make_shared<DfxSymbols>();

    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);

    bool ret = UnwindWithContext(as, context, symbol, skipFrameNum + 1, maxFrameNums);
    unw_destroy_local_address_space(as);
    return ret;
}

bool DwarfUnwinder::UnwindWithContext(unw_addr_space_t as, unw_context_t& context,
    std::shared_ptr<DfxSymbols> symbol, size_t skipFrameNum, size_t maxFrameNums)
{
    if (as == nullptr) {
        return false;
    }

    unw_cursor_t cursor;
    unw_init_local_with_as(as, &cursor, &context);
    size_t index = 0;
    size_t curIndex = 0;
    unw_word_t prevFrameSp = 0;
    unw_word_t prevFramePc = 0;
    do {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFrameNum) {
            uint64_t skipPc = 0;
            skipPc = unw_get_rel_pc(&cursor);
            struct map_info* skipMap = unw_get_map(&cursor);
            std::string skipMapName = "";
            if ((skipMap != nullptr) && (strlen(skipMap->path) < LINE_BUF_SIZE - 1)) {
                skipMapName = std::string(skipMap->path);
            }
            DFXLOG_INFO(" skip frame #%zu pc %" PRIu64 " %s", index, skipPc, skipMapName.c_str());
            index++;
            continue;
        }
        curIndex = index - skipFrameNum;

        DfxFrame frame;
        frame.index = curIndex;
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(frame.pc)))) {
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(frame.sp)))) {
            break;
        }

        if (prevFrameSp == frame.sp && prevFramePc == frame.pc) {
            break;
        }
        prevFrameSp = frame.sp;
        prevFramePc = frame.pc;
        frame.relPc = unw_get_rel_pc(&cursor);
        unw_word_t sz = unw_get_previous_instr_sz(&cursor);
        if ((frame.index > 0) && (frame.relPc > sz)) {
            frame.relPc -= sz;
            frame.pc -= sz;
#if defined(__arm__)
            unw_set_adjust_pc(&cursor, frame.pc);
#endif
        }

        struct map_info* map = unw_get_map(&cursor);
        bool isValidFrame = true;
        if ((map != nullptr) && (strlen(map->path) < LINE_BUF_SIZE - 1)) {
            UpdateFrameFuncName(as, symbol, frame);
            frame.mapName = std::string(map->path);
            if (frame.mapName.find(".hap") != std::string::npos) {
                char libraryName[PATH_LEN] = { 0 };
                if (unw_get_library_name_by_map(map, libraryName, PATH_LEN - 1) == 0) {
                    frame.mapName = frame.mapName + "!" + std::string(libraryName);
                }
            }
        } else {
            isValidFrame = false;
        }

        if (isValidFrame && (frame.relPc == 0) && (frame.mapName.find("Ark") == std::string::npos)) {
            isValidFrame = false;
        }

        if (frame.index < MIN_VALID_FRAME_COUNT || isValidFrame) {
            frames_.emplace_back(frame);
        } else {
            break;
        }

        index++;
    } while ((unw_step(&cursor) > 0) && (curIndex < maxFrameNums));
    return (frames_.size() > 0);
}
} // namespace HiviewDFX
} // namespace OHOS
