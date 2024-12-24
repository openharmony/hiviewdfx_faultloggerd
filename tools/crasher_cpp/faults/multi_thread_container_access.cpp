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

#include "multi_thread_container_access.h"

#include <cinttypes>
#include <cstdlib>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace OHOS::HiviewDFX;

static constexpr int MAP_INDEX_LEN = 10;
static constexpr int MAX_LOOP_SIZE = 10000;
static constexpr int THREAD_SIZE = 10;

using TaskFunction = std::function<void(std::shared_ptr<MultiThreadContainerAccess>)>;

int MultiThreadAccess(const TaskFunction& manipulate)
{
    auto testcase = std::make_shared<MultiThreadContainerAccess>();
    testcase->Print();
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_SIZE; i++) {
        std::thread th(
            [testcase, manipulate] {
                manipulate(testcase);
            });
        threads.push_back(std::move(th));
    }

    for (auto& th : threads) {
        th.join();
    }
    return 0;
}

int MultiThreadVectorAccess()
{
    auto multiThreadVectorAccess = [](std::shared_ptr<MultiThreadContainerAccess> testcase) {
        for (int i = 0; i < MAX_LOOP_SIZE; i++) {
            testcase->ManipulateVector();
        }
        testcase->Print();
    };
    return MultiThreadAccess(multiThreadVectorAccess);
}

int MultiThreadMapAccess()
{
    auto multiThreadMapAccess = [](std::shared_ptr<MultiThreadContainerAccess> testcase) {
        for (int i = 0; i < MAX_LOOP_SIZE; i++) {
            testcase->ManipulateMap();
        }
        testcase->Print();
    };
    return MultiThreadAccess(multiThreadMapAccess);
}

int MultiThreadListAccess()
{
    auto multiThreadListAccess = [](std::shared_ptr<MultiThreadContainerAccess> testcase) {
        for (int i = 0; i < MAX_LOOP_SIZE; i++) {
            // may crash inside loop
            testcase->ManipulateList();
            testcase->Print();
        }
    };
    return MultiThreadAccess(multiThreadListAccess);
}

namespace OHOS {
namespace HiviewDFX {
std::string MultiThreadContainerAccess::GenerateStr()
{
    constexpr int dictLen = 26;
    constexpr int len = 10;
    const char dict[dictLen] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u',
        'v', 'w', 'x', 'y', 'z' };
    std::string result = "";
    for (int i = 0; i < len; i++) {
        result = result + dict[rand() % dictLen];
    }
    return result;
}

void MultiThreadContainerAccess::ManipulateVector()
{
    if ((rand() % MANIPULATION_TYPE) == ADD) {
        strVector_.push_back(GenerateStr());
    } else if (!strVector_.empty()) {
        strVector_.pop_back();
    }
}

void MultiThreadContainerAccess::ManipulateMap()
{
    if ((rand() % MANIPULATION_TYPE) == ADD) {
        strMap_[rand() % MAP_INDEX_LEN] = GenerateStr();
    } else {
        strMap_.erase(rand() % MAP_INDEX_LEN);
    }
}

void MultiThreadContainerAccess::ManipulateList()
{
    if ((rand() % MANIPULATION_TYPE) == ADD) {
        strList_.push_back(GenerateStr());
    } else if (!strList_.empty()) {
        strList_.pop_back();
    }
}

void MultiThreadContainerAccess::Print() const
{
    printf("MultiThreadContainerAccess::Print begin\n");
    for (const auto& value : strList_) {
        printf("List:%s\n", value.c_str());
    }

    for (const auto& value : strVector_) {
        printf("Vector:%s\n", value.c_str());
    }

    for (const auto& entry : strMap_) {
        printf("Map: key[%d]:value[%s]\n", entry.first, entry.second.c_str());
    }
    printf("MultiThreadContainerAccess::Print end\n");
}
} // namespace HiviewDFX
} // namespace OHOS
