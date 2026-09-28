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

#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include <securec.h>

#include <cstdio>
#include <sys/ucontext.h>
#include <ucontext.h>

#include "dfx_maps.h"
#include "thread_context.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace {
static ucontext_t CreateUcontext()
{
    ucontext_t ucp;
#if defined(__arm__)
    ucp.uc_mcontext.arm_sp = (unsigned long)__builtin_frame_address(0);
#elif defined(__aarch64__)
    ucp.uc_mcontext.sp = (unsigned long)__builtin_frame_address(0);
#endif
    return ucp;
}
}

/**
 * @tc.name: AccessMem001
 * @tc.desc: test AccessMem
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMem001, testing::ext::TestSize.Level0)
{
    // getcontext
    ucontext_t ucp = CreateUcontext();

    // init LocalThreadContextMix
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    instance.CollectThreadContext(gettid());
    instance.CopyRegister((void*)&ucp);

    // addr + sizeof(uintptr_t) overflow
    uintptr_t addr = (uintptr_t)(-1) - sizeof(uintptr_t) / 2;
    uintptr_t val = -1;
    ASSERT_EQ(instance.AccessMem(addr, &val), -1);
}

/**
 * @tc.name: AccessMem002
 * @tc.desc: test AccessMem
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMem002, testing::ext::TestSize.Level0)
{
    // getcontext
    ucontext_t ucp = CreateUcontext();

    // init LocalThreadContextMix
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    instance.CollectThreadContext(gettid());
    instance.CopyRegister((void*)&ucp);

    // addr < sp_
#if defined(__arm__)
    uintptr_t addr = ucp.uc_mcontext.arm_sp - 1;
#elif defined(__aarch64__)
    uintptr_t addr = ucp.uc_mcontext.sp - 1;
#endif
    uintptr_t val = -1;
    ASSERT_EQ(instance.AccessMem(addr, &val), -1);
}

/**
 * @tc.name: AccessMem003
 * @tc.desc: test AccessMem
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMem003, testing::ext::TestSize.Level0)
{
    // getcontext
    ucontext_t ucp = CreateUcontext();

    // init LocalThreadContextMix
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    instance.SetStackForward(0);
    instance.SetStackBuf(std::vector<uint8_t>(STACK_BUFFER_SIZE, 0));
    instance.SetMaps(DfxMaps::Create());
#if defined(__arm__)
    instance.SetSp(ucp.uc_mcontext.arm_sp);
#elif defined(__aarch64__)
    instance.SetSp(ucp.uc_mcontext.sp);
#endif

    // sp_ <= addr <= sp_ + STACK_BUFFER_SIZE - sizeof(uintptr_t)
#if defined(__arm__)
    uintptr_t addr = ucp.uc_mcontext.arm_sp;
#elif defined(__aarch64__)
    uintptr_t addr = ucp.uc_mcontext.sp;
#endif
    uintptr_t val = -1;
    ASSERT_EQ(instance.AccessMem(addr, &val), 0);
    ASSERT_EQ(val, 0);

    val = -1;
#if defined(__arm__)
    addr = ucp.uc_mcontext.arm_sp + STACK_BUFFER_SIZE - sizeof(uintptr_t) * 10;
#elif defined(__aarch64__)
    addr = ucp.uc_mcontext.sp + STACK_BUFFER_SIZE - sizeof(uintptr_t) * 10;
#endif
    ASSERT_EQ(instance.AccessMem(addr, &val), 0);
    ASSERT_EQ(val, 0);

    val = -1;
#if defined(__arm__)
    addr = ucp.uc_mcontext.arm_sp + STACK_BUFFER_SIZE - sizeof(uintptr_t);
#elif defined (__aarch64__)
    addr = ucp.uc_mcontext.sp + STACK_BUFFER_SIZE - sizeof(uintptr_t);
#endif
    ASSERT_EQ(instance.AccessMem(addr, &val), 0);
    ASSERT_EQ(val, 0);
}

/**
 * @tc.name: AccessMem004
 * @tc.desc: test AccessMem
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMem004, testing::ext::TestSize.Level0)
{
    // getcontext
    ucontext_t ucp = CreateUcontext();

    // init LocalThreadContextMix
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    instance.CollectThreadContext(gettid());
    instance.CopyRegister((void*)&ucp);

    // addr > sp_ + STACK_BUFFER_SIZE - sizeof(uintptr_t)
#if defined(__arm__)
    uintptr_t addr = ucp.uc_mcontext.arm_sp + STACK_BUFFER_SIZE - sizeof(uintptr_t) + 1;
#elif defined (__aarch64__)
    uintptr_t addr = ucp.uc_mcontext.sp + STACK_BUFFER_SIZE - sizeof(uintptr_t) + 1;
#endif
    uintptr_t val = -1;
    ASSERT_EQ(instance.AccessMem(addr, &val), -1);
}

/**
 * @tc.name: LocalThreadContextMixSetSp001
 * @tc.desc: test LocalThreadContextMix SetSp sets sp value
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, SetSp001, testing::ext::TestSize.Level2)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    uintptr_t testSp = 0x10000;
    instance.SetSp(testSp);
    EXPECT_EQ(instance.sp_, testSp);
    instance.SetSp(0);
    EXPECT_EQ(instance.sp_, 0);
}

/**
 * @tc.name: AccessMemStackWindow001
 * @tc.desc: an injected buffer is fully readable through AccessMem
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMemStackWindow001, testing::ext::TestSize.Level0)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    constexpr uintptr_t sp = 0x10000;
    std::vector<uint8_t> buf(0x100, 0);
    const uint32_t magic = 0x12345678;
    ASSERT_EQ(memcpy_s(buf.data() + 0x10, buf.size() - 0x10, &magic, sizeof(magic)), 0);
    instance.SetStackForward(0);
    instance.SetStackBuf(buf);
    instance.SetSp(sp);
    instance.SetMaps(DfxMaps::Create());
    uintptr_t val = 0;
    EXPECT_EQ(instance.AccessMem(sp + 0x10, &val), 0);
    EXPECT_EQ(val, static_cast<uintptr_t>(magic));
    EXPECT_EQ(instance.AccessMem(sp + buf.size() - sizeof(uintptr_t), &val), 0);
}

/**
 * @tc.name: AccessMemStackWindow002
 * @tc.desc: reads past the actually copied length fail even inside the 64K window
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMemStackWindow002, testing::ext::TestSize.Level0)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    constexpr uintptr_t sp = 0x10000;
    std::vector<uint8_t> buf(STACK_BUFFER_SIZE, 0);
    instance.SetStackForward(0);
    instance.SetStackBuf(buf);
    // copied bytes end at 0x558; the rest of the 64K buffer is zero fill
    instance.stackCopiedLen_ = 0x558;
    instance.SetSp(sp);
    instance.SetMaps(DfxMaps::Create());
    uintptr_t val = 1;
    EXPECT_EQ(instance.AccessMem(sp + 0x558, &val), -1);
    // .ARM.exidx offset observed in the field case
    EXPECT_EQ(instance.AccessMem(sp + 0x232c, &val), -1);
    EXPECT_EQ(val, 0); // failed read must not write to *val
    EXPECT_EQ(instance.AccessMem(sp + 0x10, &val), 0);
    EXPECT_EQ(val, 0);
}

/**
 * @tc.name: AccessMemStackWindow003
 * @tc.desc: reads beyond the 64K window fail
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMemStackWindow003, testing::ext::TestSize.Level0)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    constexpr uintptr_t sp = 0x10000;
    std::vector<uint8_t> buf(STACK_BUFFER_SIZE, 0);
    instance.SetStackForward(0);
    instance.SetStackBuf(buf);
    instance.SetSp(sp);
    instance.SetMaps(DfxMaps::Create());
    uintptr_t val = 0;
    EXPECT_EQ(instance.AccessMem(sp + STACK_BUFFER_SIZE, &val), -1);
    EXPECT_EQ(instance.AccessMem(sp + STACK_BUFFER_SIZE - sizeof(uintptr_t), &val), 0);
    // addr + sizeof(uintptr_t) > sp_ + STACK_BUFFER_SIZE
    EXPECT_EQ(instance.AccessMem(sp + STACK_BUFFER_SIZE - 3, &val), -1);
}

/**
 * @tc.name: AccessMemStackWindow004
 * @tc.desc: after release every buffer read fails
 * @tc.type: FUNC
 */
HWTEST(LocalThreadContextMixTest, AccessMemStackWindow004, testing::ext::TestSize.Level0)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    instance.ReleaseCollectThreadContext();
    constexpr uintptr_t sp = 0x10000;
    std::vector<uint8_t> buf(0x100, 0);
    instance.SetStackForward(0);
    instance.SetStackBuf(buf);
    instance.SetSp(sp);
    instance.SetMaps(DfxMaps::Create());
    uintptr_t val = 0;
    EXPECT_EQ(instance.AccessMem(sp + 0x10, &val), 0);
    instance.ReleaseCollectThreadContext();
    EXPECT_EQ(instance.AccessMem(sp + 0x10, &val), -1);
}
} // namespace HiviewDFX
} // namepsace OHOS

