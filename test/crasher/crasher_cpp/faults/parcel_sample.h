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
#ifndef DFX_FAULTS_PARCEL_SAMPLE_H
#define DFX_FAULTS_PARCEL_SAMPLE_H

#include <cstdint>
#include <map>
#include <string>

#include "parcel.h"

namespace OHOS {
namespace HiviewDFX {
class ParcelSample : public Parcelable {
public:
    int64_t member0 {0};
    int32_t member1 {-1};
    int32_t member2 {-1};
    std::string member3 {""};

    bool Marshalling(Parcel &parcel) const override;
    static ParcelSample* Unmarshalling(Parcel &parcel);

    ParcelSample() {};
    ~ParcelSample() {};
};
}
} // namespace OHOS
#endif // FAULTLOG_INFO_OHOS_H
