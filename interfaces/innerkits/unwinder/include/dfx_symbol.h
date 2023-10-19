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
#ifndef DFX_SYMBOL_H
#define DFX_SYMBOL_H

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "string_view_util.h"

namespace OHOS {
namespace HiviewDFX {
struct DfxSymbol {
    uint64_t funcVaddr_ = 0;
    uint64_t offsetToVaddr_ = 0;
    uint64_t fileVaddr_ = 0;
    uint64_t taskVaddr_ = 0;
    uint64_t size_ = 0;
    int32_t symbolFileIndex_ = -1; // symbols file index, used to report protobuf file
    int32_t index_ = -1;
    STRING_VIEW name_ = "";
    STRING_VIEW demangle_ = ""; // demangle string
    STRING_VIEW module_ = "";   // maybe empty
    STRING_VIEW comm_ = "";     // we need a comm name like comm@0x1234
    mutable STRING_VIEW unknow_ = "";
    mutable bool matched_ = false; // if some callstack match this
    int32_t hit_ = 0;

    // elf use this
    DfxSymbol(uint64_t vaddr, uint64_t size, const std::string &name, const std::string &demangle,
           const std::string module)
        : funcVaddr_(vaddr),
          fileVaddr_(vaddr),
          size_(size),
          name_(StringViewHold::Get().Hold(name)),
          demangle_(StringViewHold::Get().Hold(demangle)),
          module_(StringViewHold::Get().Hold(module)) {}
    DfxSymbol(uint64_t vaddr, uint64_t size, const std::string &name, const std::string &module)
        : DfxSymbol(vaddr, size, name, name, module) {}

    // kernel use this
    DfxSymbol(uint64_t vaddr, const std::string &name, const std::string &module)
        : DfxSymbol(vaddr, 0, name, name, module) {}

    // Symbolic use this
    DfxSymbol(uint64_t taskVaddr = 0, const std::string &comm = "")
        : taskVaddr_(taskVaddr), comm_(comm)
    {
    }

    DfxSymbol(const DfxSymbol &other) = default;

    DfxSymbol& operator=(const DfxSymbol& other) = default;

    inline bool Equal(const DfxSymbol &b) const
    {
        return ((funcVaddr_ == b.funcVaddr_) && (demangle_ == b.demangle_));
    }

    inline bool operator==(const DfxSymbol &b) const
    {
        return Equal(b);
    }

    inline bool operator!=(const DfxSymbol &b) const
    {
        return !Equal(b);
    }

    inline bool IsValid() const
    {
        return !module_.empty();
    }

    STRING_VIEW GetName() const
    {
        if (!demangle_.empty()) {
            return demangle_;
        }
        if (!name_.empty()) {
            return name_;
        }
        if (unknow_.empty()) {
            std::stringstream ss;
            if (!module_.empty()) {
                ss << module_ << "+0x" << std::hex << fileVaddr_;
            } else {
                ss << comm_ << "@0x" << std::hex << taskVaddr_;
            }
            unknow_ = StringViewHold::Get().Hold(ss.str());
        }
        return unknow_;
    }

    inline std::string ToString() const
    {
        std::stringstream ss;
        if (fileVaddr_ != 0) {
            ss << "0x" << std::hex << fileVaddr_;
        } else {
            ss << "0x" << std::hex << taskVaddr_;
        }
        ss << " " << GetName();
        return ss.str();
    };

    std::string ToDebugString() const
    {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(sizeof(funcVaddr_) * 2); // 2 : a multiplicand
        ss << std::hex << funcVaddr_;
        ss << "|";
        ss << std::setfill('0') << std::setw(sizeof(size_)) << size_;
        ss << "|";
        ss << demangle_ << "|";
        ss << name_ << "|";
        ss << (matched_ ? "matched" : "");
        ss << " unknowname:" << unknow_.size();
        ss << " task:" << (comm_.size() > 0 ? comm_ : "");
        ss << "@" << taskVaddr_;
        ss << " file:" << (module_.size() > 0 ? module_ : "");
        ss << "@" << fileVaddr_;
        return ss.str();
    };

    inline bool Contain(uint64_t addr) const
    {
        if (size_ == 0) {
            return funcVaddr_ <= addr;
        } else {
            return (funcVaddr_ <= addr) && ((funcVaddr_ + size_) > addr);
        }
    }

    // The range [first, last) must be partitioned with respect to the expression
    // !(value < element) or !comp(value, element)
    static bool ValueLessThen(uint64_t vaddr, const DfxSymbol &a)
    {
        return vaddr < a.funcVaddr_;
    }

    static bool ValueLessEqual(uint64_t vaddr, const DfxSymbol &a)
    {
        return vaddr <= a.funcVaddr_;
    }
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
