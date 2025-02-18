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
#include <pthread.h>
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

    const char* Hold(STRING_VIEW view)
    {
        pthread_spin_lock(&spin_lock_);
        if (view.size() == 0) {
            pthread_spin_unlock(&spin_lock_);
            return "";
        }

        char *p = new (std::nothrow) char[view.size() + 1];
        if (p == nullptr) {
            pthread_spin_unlock(&spin_lock_);
            return "";
        }
        if (memset_s(p, view.size() + 1, '\0', view.size() + 1) != 0) {
            pthread_spin_unlock(&spin_lock_);
            return "";
        }
        std::copy(view.data(), view.data() + view.size(), p);
        views_.emplace_back(p);
        pthread_spin_unlock(&spin_lock_);
        return p;
    }

    // only use in UT
    void Clean()
    {
        pthread_spin_lock(&spin_lock_);
        for (auto &p : views_) {
            delete[] p;
        }
        views_.clear();
        pthread_spin_unlock(&spin_lock_);
    }
private:
    StringViewHold()
    {
        pthread_spin_init(&spin_lock_, PTHREAD_PROCESS_PRIVATE);
    }
    ~StringViewHold()
    {
        Clean();
        pthread_spin_destroy(&spin_lock_);
    }

    std::vector<char *> views_;
    pthread_spinlock_t spin_lock_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif