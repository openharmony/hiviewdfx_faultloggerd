/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "parcel_sample.h"

#include "parcel.h"

namespace OHOS {
namespace HiviewDFX {
bool ParcelSample::Marshalling(Parcel& parcel) const
{
    if (!parcel.WriteInt64(member0)) {
        return false;
    }

    if (!parcel.WriteInt32(member1)) {
        return false;
    }

    if (!parcel.WriteInt32(member2)) {
        return false;
    }

    if (!parcel.WriteString(member3)) {
        return false;
    }

    return true;
}

ParcelSample* ParcelSample::Unmarshalling(Parcel& parcel)
{
    ParcelSample* ret = new ParcelSample();
    if (!parcel.ReadInt64(ret->member0)) {
        goto error;
    }

    if (!parcel.ReadInt32(ret->member1)) {
        goto error;
    }

    if (!parcel.ReadInt32(ret->member2)) {
        goto error;
    }

    if (!parcel.ReadString(ret->member3)) {
        goto error;
    }
    return ret;

error:
    delete ret;
    ret = nullptr;
    return nullptr;
}
}  // namespace hiview
}  // namespace OHOS
