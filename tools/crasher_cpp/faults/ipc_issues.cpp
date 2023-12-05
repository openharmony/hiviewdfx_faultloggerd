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

#include "ipc_issues.h"

#include <memory>

#include "message_parcel.h"

#include "parcel_sample.h"
#include "remote_object_interface.h"
#include "remote_object_interface_stub.h"
#include "remote_object_interface_proxy.h"

namespace OHOS {
namespace HiviewDFX {
int IPCIssues::SptrMismatch()
{
    printf("IPCIssues::SptrMismatch begin\n");
    {
        MessageParcel data;
        auto stub = new RemoteObjectInterfaceStub();
        sptr<IRemoteObject> sendObj = stub->AsObject();
        printf("sendObj:%p count:%d.\n", sendObj.GetRefPtr(), sendObj->GetSptrRefCount());

        sptr<IRemoteObject> sendObj2 = stub;
        printf("sendObj2:%p count:%d.\n", sendObj2.GetRefPtr(), sendObj2->GetSptrRefCount());
        delete stub;
        sendObj = nullptr;
        sendObj2 = nullptr;
    }
    printf("IPCIssues::SptrMismatch end\n");
    return 0;
}

int IPCIssues::SptrAndSharedPtrMixUsage()
{
    printf("IPCIssues::SptrAndSharedPtrMixUsage begin.\n");
    {
        auto sendObj = std::make_shared<RemoteObjectInterfaceStub>();
        printf("sendObj:%p.\n", sendObj.get());
        delete sendObj.get();
        sendObj = nullptr;
    }
    printf("IPCIssues::SptrAndSharedPtrMixUsage end.\n");
    return 0;
}

int IPCIssues::ParcelReadWriteMismatch()
{
    MessageParcel data;
    if (!data.WriteInterfaceToken(RemoteObjectInterfaceProxy::GetDescriptor())) {
        printf("Failed to write ipc interface.\n");
        return 0;
    }

    ParcelSample sample;
    if (!sample.Marshalling(data)) {
        printf("Failed to write sample content.\n");
        return 0;
    }

    auto stub = new RemoteObjectInterfaceStub();
    data.WriteRemoteObject(stub->AsObject());
    data.WriteInt32(1);
    sptr<IRemoteObject> recvObj = data.ReadRemoteObject();
    if (recvObj == nullptr) {
        printf("Failed to read remote object.\n");
    }

    sptr<ParcelSample> samplePtr = ParcelSample::Unmarshalling(data);
    delete samplePtr.GetRefPtr();
    samplePtr = nullptr;
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
