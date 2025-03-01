/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef DFX_FAULTS_REMOTE_OBJECT_INTERFACE_PROXY_H
#define DFX_FAULTS_REMOTE_OBJECT_INTERFACE_PROXY_H

#include "iremote_proxy.h"
#include "nocopyable.h"

#include "remote_object_interface.h"

namespace OHOS {
namespace HiviewDFX {
class RemoteObjectInterfaceProxy : public IRemoteProxy<RemoteObjectInterface> {
public:
    explicit RemoteObjectInterfaceProxy(const sptr<IRemoteObject>& impl)
        : IRemoteProxy<RemoteObjectInterface>(impl) {}
    virtual ~RemoteObjectInterfaceProxy() = default;
    DISALLOW_COPY_AND_MOVE(RemoteObjectInterfaceProxy);

    bool Print() override;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif // DFX_FAULTS_REMOTE_OBJECT_INTERFACE_PROXY_H