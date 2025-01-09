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

#ifndef DFX_CRASHER
#define DFX_CRASHER
static const int ARG1024 = 1024;
static const int ARG128 = 128;

void ProgramVolatile(void)
{
#if defined(__arm__)
    __asm__ volatile (
        "mov r0, #0x00\n mov lr, pc\n bx r0\n"
    );
#elif defined(__aarch64__)
    __asm__ volatile (
        "movz x0, #0x0\n"
        "adr x30, .\n"
        "br x0\n"
    );
#elif defined(__loongarch_lp64)
    __asm__ volatile (
        "pcaddi $ra, 0\n"
        "jr $zero\n"
    );
#endif
}

void IllegalVolatile(void)
{
#if defined(__aarch64__)
    __asm__ volatile(".word 0\n");
#elif defined(__arm__)
    __asm__ volatile(".word 0xe7f0def0\n");
#elif defined(__x86_64__)
    __asm__ volatile("ud2\n");
#elif defined(__loongarch_lp64)
    // Effective illegal instruction on LoongArch64: amswap.w $zero, $ra, $zero
    __asm__ volatile(".word 0x38600400\n");
#else
#error
#endif
}

#endif // DFX_CRASHER
