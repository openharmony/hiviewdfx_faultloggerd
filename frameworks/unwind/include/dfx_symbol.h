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
#include "cpp_define.h"

namespace OHOS {
namespace HiviewDFX {
class StringViewHold {
public:
    static StringViewHold &Get()
    {
        static StringViewHold instance;
        return instance;
    }

    ~StringViewHold()
    {
        Clean();
    }

    const char* Hold(STRING_VIEW view)
    {
        if (view.size() == 0) {
            return "";
        }

        char *p = new (std::nothrow) char[view.size() + 1];
        if (p == nullptr) {
            return "";
        }
        p[view.size()] = '\0';
        std::copy(view.data(), view.data() + view.size(), p);
        views_.emplace_back(p);
        return p;
    }

    // only use in UT
    void Clean()
    {
        for (auto &p : views_) {
            delete[] p;
        }
        views_.clear();
    }
private:
    std::vector<char *> views_;
};

struct DfxSymbol {
    uint64_t funcVaddr_ = 0;
    uint64_t offsetToVaddr_ = 0;
    uint64_t fileVaddr_ = 0;
    uint64_t taskVaddr_ = 0;
    uint64_t len_ = 0;
    int32_t symbolFileIndex_ = -1; // symbols file index, used to report protobuf file
    int32_t index_ = -1;
    STRING_VIEW name_ = "";
    STRING_VIEW demangle_ = ""; // demangle string
    STRING_VIEW module_ = "";   // maybe empty
    STRING_VIEW comm_ = "";     // we need a comm name like comm@0x1234
    mutable STRING_VIEW unknow_ = "";
    mutable bool matched_ = false; // if some callstack match this
    int32_t hit_ = 0;

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

    // elf use this
    DfxSymbol(uint64_t vaddr, uint64_t len, const std::string &name, const std::string &demangle,
           const std::string module)
        : funcVaddr_(vaddr),
          fileVaddr_(vaddr),
          len_(len),
          name_(name),
          demangle_(demangle),
          module_(module) {}
    DfxSymbol(uint64_t vaddr, uint64_t len, const std::string &name, const std::string &module)
        : DfxSymbol(vaddr, len, name, name, module) {}

    // kernel use this
    DfxSymbol(uint64_t vaddr, const std::string &name, const std::string &module)
        : DfxSymbol(vaddr, 0, name, name, module) {}

    // Symbolic use this
    DfxSymbol(uint64_t taskVaddr = 0, const std::string &comm = "")
        : taskVaddr_(taskVaddr), comm_(comm) {}

    // copy
    DfxSymbol(const DfxSymbol &other) = default;

    DfxSymbol& operator=(const DfxSymbol& other) = default;

    static bool SameVaddr(const DfxSymbol &a, const DfxSymbol &b)
    {
        return (a.funcVaddr_ == b.funcVaddr_);
    }
    bool Same(const DfxSymbol &b) const
    {
        return (funcVaddr_ == b.funcVaddr_ and demangle_ == b.demangle_);
    }
    bool operator==(const DfxSymbol &b) const
    {
        return Same(b);
    }

    bool operator!=(const DfxSymbol &b) const
    {
        return !Same(b);
    }

    bool isValid() const
    {
        return !module_.empty();
    }

    void SetMatchFlag() const
    {
        matched_ = true;
    }

    inline bool HasMatched() const
    {
        return matched_;
    }

    std::string ToString() const
    {
        return "";
    };

    std::string ToDebugString() const
    {
        return "";
    };

    bool Contain(uint64_t addr) const
    {
        if (len_ == 0) {
            return funcVaddr_ <= addr;
        } else {
            return (funcVaddr_ <= addr) and ((funcVaddr_ + len_) > addr);
        }
    }

    // The range [first, last) must be partitioned with respect to the expression !(value < element)
    // or !comp(value, element)
    static bool ValueLessThen(uint64_t vaddr, const DfxSymbol &a)
    {
        return vaddr < a.funcVaddr_;
    }
    static bool ValueLessEqual(uint64_t vaddr, const DfxSymbol &a)
    {
        return vaddr <= a.funcVaddr_;
    }
    static bool CompareLessThen(const DfxSymbol &a, const DfxSymbol &b)
    {
        return a.funcVaddr_ < b.funcVaddr_; // we should use vaddr to sort
    };
    static bool CompareByPointer(const DfxSymbol *a, const DfxSymbol *b)
    {
        return a->funcVaddr_ < b->funcVaddr_; // we should use vaddr to sort
    };
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
