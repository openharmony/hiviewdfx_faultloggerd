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

#ifndef DUMP_CATCHER_DEMO_H
#define DUMP_CATCHER_DEMO_H

#define NOINLINE __attribute__((noinline))

#define GEN_TEST_FUNCTION(FuncNumA, FuncNumB)          \
    __attribute__((noinline)) int TestFunc##FuncNumA() \
    {                                                  \
        return TestFunc##FuncNumB();                   \
    }

// test functions for callstack depth test
int TestFunc0(void);
int TestFunc1(void);
int TestFunc2(void);
int TestFunc3(void);
int TestFunc4(void);
int TestFunc5(void);
int TestFunc6(void);
int TestFunc7(void);
int TestFunc8(void);
int TestFunc9(void);

#endif // DUMP_CATCHER_DEMO_H
