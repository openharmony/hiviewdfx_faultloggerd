/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef DUMP_INFO_FACTORY_H
#define DUMP_INFO_FACTORY_H
#include <functional>
#include <string>
#include <map>
#include <memory>
#include "dump_info.h"
namespace OHOS {
namespace HiviewDFX {
using CreateObjectFunc = std::shared_ptr<DumpInfo>(*)();
class DumpInfoFactory {
public:
    static DumpInfoFactory& GetInstance()
    {
        static DumpInfoFactory instance;
        return instance;
    }
    DumpInfoFactory(const DumpInfoFactory&) = delete;
    DumpInfoFactory &operator=(const DumpInfoFactory&) = delete;
    int RegisterClass(const std::string& className, CreateObjectFunc func)
    {
        classMap_[className] = func;
        return 0;
    }
    std::shared_ptr<DumpInfo> CreateObject(const std::string& className)
    {
        auto iter = classMap_.find(className);
        if (iter != classMap_.end()) {
            return (iter->second)();
        }
        return nullptr;
    }
private:
    DumpInfoFactory() = default;
    std::map<std::string, CreateObjectFunc> classMap_;
};
     
#define REGISTER_DUMP_INFO_CLASS(ClassName) \
    static int instance##ClassName = \
        DumpInfoFactory::GetInstance().RegisterClass(#ClassName, ClassName::CreateInstance)
}
}
#endif