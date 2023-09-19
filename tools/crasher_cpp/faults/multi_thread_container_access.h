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

#ifndef DFX_FAULTS_MULTI_THREAD_CONTAINER_ACCESS_H
#define DFX_FAULTS_MULTI_THREAD_CONTAINER_ACCESS_H

#include <cinttypes>
#include <cstdlib>
#include <list>
#include <map>
#include <string>
#include <vector>

int MultiThreadVectorAccess();
int MultiThreadMapAccess();
int MultiThreadListAccess();

namespace OHOS {
namespace HiviewDFX {
class MultiThreadContainerAccess {
public:
    MultiThreadContainerAccess() {};
    ~MultiThreadContainerAccess() {};

    void ManipulateVector();
    void ManipulateMap();
    void ManipulateList();
    void Print();

private:
    static constexpr int MANIPULATION_TYPE = 2;
    static constexpr int ADD = 0;
    static constexpr int REMOVE = 1;
    std::string GenerateStr();

private:
    std::vector<std::string> strVector_;
    std::map<int, std::string> strMap_;
    std::list<std::string> strList_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
