/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef DFX_LOG_PUBLIC_H
#define DFX_LOG_PUBLIC_H

#include <array>

#ifndef is_ohos_lite
namespace OHOS {
namespace HiviewDFX {
template<typename I, typename O>
constexpr void Copy(I first, I last, O result)
{
    while (first != last) {
        *result++ = *first++;
    }
}

template<typename LHS, typename RHS>
constexpr auto ConcatStr(LHS&& lhs, RHS&& rhs)
{
    std::array<char, sizeof(lhs) + sizeof(rhs) - 1> res {};
    std::size_t i = 0;
    for (auto it = std::begin(lhs); it != std::end(lhs) && *it; ++it) {
        res[i++] = *it;
    }
    for (auto it = std::begin(rhs); it != std::end(rhs) && *it; ++it) {
        res[i++] = *it;
    }
    return res;
}

// 编译时计算: 直接得到func<line>:字符串，避免运行时格式化开销
template<std::size_t N, int... D>
struct NumStr : NumStr<N / 10, N % 10, D...> { // 10 : decimal
};

template<int ...D>
struct NumStr<0, D...> {
    static_assert(((D >= 0 && D <= 9) && ...)); // 9 : decimal
    constexpr static std::size_t len = sizeof...(D);
    constexpr static char str[] { D + '0'... };
};

template<char... Cs>
struct CharList {
    static constexpr std::array value { Cs..., '\0'};
};

// 编译时计算: 字符串替换 %d -> %{public}d
// ohos默认hilog打印<private>而不是具体值
template<typename In, typename Out = OHOS::HiviewDFX::CharList<>>
struct FmtToPublic {
    static constexpr auto value = Out::value;
};

template<char C, char... Cs, char... O>
struct FmtToPublic<CharList<C, Cs...>, CharList<O...>> : FmtToPublic<CharList<Cs...>, CharList<O..., C>> {};

// case: %{, skip
template<char... Cs, char... O>
struct FmtToPublic<CharList<'%', '{', Cs...>, CharList<O...>>
    : FmtToPublic<CharList<Cs...>, CharList<O..., '%', '{'>> {};

// case: %%, skip
template<char... Cs, char... O>
struct FmtToPublic<CharList<'%', '%', Cs...>, CharList<O...>>
    : FmtToPublic<CharList<Cs...>, CharList<O..., '%', '%'>> {};

// case: %d, %u, add {public}
template<char... Cs, char... O>
struct FmtToPublic<CharList<'%', Cs...>, CharList<O...>>
    : FmtToPublic<CharList<Cs...>, CharList<O..., '%', '{', 'p', 'u', 'b', 'l', 'i', 'c', '}'>> {};
} // nameapace HiviewDFX
} // nameapace OHOS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
template<typename CharT, CharT... Cs>
constexpr auto operator"" _DfxToPublic()
{
    return OHOS::HiviewDFX::FmtToPublic<OHOS::HiviewDFX::CharList<Cs...>>::value;
};
#pragma clang diagnostic pop
#endif // is_ohos_lite
#endif