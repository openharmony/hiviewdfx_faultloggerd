/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef BYTE_ORDER_H
#define BYTE_ORDER_H

#include <endian.h>

#if defined(__LITTLE_ENDIAN)
#define UNWIND_LITTLE_ENDIAN __LITTLE_ENDIAN
#elif defined(_LITTLE_ENDIAN)
#define UNWIND_LITTLE_ENDIAN _LITTLE_ENDIAN
#elif defined(LITTLE_ENDIAN)
#define UNWIND_LITTLE_ENDIAN LITTLE_ENDIAN
#else
#define UNWIND_LITTLE_ENDIAN 1234
#endif

#if defined(__BIG_ENDIAN)
#define UNWIND_BIG_ENDIAN __BIG_ENDIAN
#elif defined(_BIG_ENDIAN)
#define UNWIND_BIG_ENDIAN _BIG_ENDIAN
#elif defined(BIG_ENDIAN)
#define UNWIND_BIG_ENDIAN BIG_ENDIAN
#else
#define UNWIND_BIG_ENDIAN 4321
#endif

#if defined(__BYTE_ORDER)
#define UNWIND_BYTE_ORDER __BYTE_ORDER
#elif defined(_BYTE_ORDER)
#define UNWIND_BYTE_ORDER _BYTE_ORDER
#elif defined(BYTE_ORDER)
#define UNWIND_BYTE_ORDER BYTE_ORDER
#else
#error Target has unknown byte ordering.
#endif

namespace OHOS {
namespace HiviewDFX {
inline int ByteOrderIsValid(int byteOrder)
{
    return byteOrder != UNWIND_BIG_ENDIAN
        && byteOrder != UNWIND_LITTLE_ENDIAN;
}

inline bool ByteOrderIsBigEndian(int byteOrder)
{
    return (byteOrder == UNWIND_BIG_ENDIAN);
}

inline bool TargetIsBigEndian(void)
{
    return ByteOrderIsBigEndian(UNWIND_BYTE_ORDER);
}
} // namespace HiviewDFX
} // namespace OHOS
#endif