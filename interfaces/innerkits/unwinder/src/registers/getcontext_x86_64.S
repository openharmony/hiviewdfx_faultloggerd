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

/*
    int _x86_64_getcontext (void* regs)
*/
.global _x86_64_getcontext
.hidden _x86_64_getcontext
.type _x86_64_getcontext, @function
_x86_64_getcontext:
  .cfi_startproc
  movq %rbp, 48(%rdi)
  movq %rbx, 24(%rdi)

  leaq 8(%rsp), %rax
  movq %rax, 56(%rdi)

  movq 0(%rsp), %rax
  movq %rax, 128(%rdi)

  xorq %rax, %rax
  retq
  .cfi_endproc