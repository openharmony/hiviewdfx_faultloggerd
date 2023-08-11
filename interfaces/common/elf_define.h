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
#ifndef ELF_DEFINE_H
#define ELF_DEFINE_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
static const std::string NOTE_GNU_BUILD_ID = ".note.gnu.build-id";
static const std::string GNU_DEBUGDATA = ".gnu_debugdata";
static const std::string EH_FRAME_HDR = ".eh_frame_hdr";
static const std::string EH_FRAME = ".eh_frame";
static const std::string ARM_EXIDX = ".ARM.exidx";
static const std::string ARM_EXTAB = ".ARM.extab";
static const std::string SHSTRTAB = ".shstrtab";
static const std::string STRTAB = ".strtab";
static const std::string SYMTAB = ".symtab";
static const std::string DYNSYM = ".dynsym";
static const std::string DYNSTR = ".dynstr";
static const std::string PLT = ".plt";

static const std::string LINKER_PREFIX = "__dl_";
static const std::string LINKER_PREFIX_NAME = "[linker]";
} // namespace HiviewDFX
} // namespace OHOS
#endif
