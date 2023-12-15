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
#ifndef STRING_VIEW_UTIL_H
#define STRING_VIEW_UTIL_H

#include <cstdio>
#include <string>
#include <securec.h>
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
        if (memset_s(p, view.size() + 1, '\0', view.size() + 1) != 0) {
            return "";
        }
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
} // namespace HiviewDFX
} // namespace OHOS
#endif
