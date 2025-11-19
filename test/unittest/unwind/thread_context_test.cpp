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
#include <gtest/gtest.h>

#include <cstdio>
#include <sys/ucontext.h>
#include <ucontext.h>

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
    instance.CollectThreadContext(gettid());
    instance.CopyRegister((void*)&ucp);

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
} // namespace HiviewDFX
} // namepsace OHOS

