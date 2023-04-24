/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains c crasher header modules. */

#ifndef DFX_CRASHER_H
#define DFX_CRASHER_H

#include <stdint.h>  // for uint64_t

#define NOINLINE __attribute__((noinline))

#define GEN_TEST_FUNCTION(FuncNumA, FuncNumB)          \
    __attribute__((noinline)) int TestFunc##FuncNumA() \
    {                                                  \
        return TestFunc##FuncNumB();                   \
    }


int RaiseAbort(void);
int Abort(void);
int RaiseBusError(void);
int RaiseFloatingPointException(void);
int RaiseIllegalInstructionException(void);
int IllegalInstructionException(void);
int RaiseSegmentFaultException(void);
int SegmentFaultException(void);
int RaiseTrapException(void);
int CrashTest(void);


void PrintUsage(void);
void *DoCrashInThread(void *inputArg);
void *SleepThread(void *argv);

uint64_t DoActionOnSubThread(const char *arg);
uint64_t ParseAndDoCrash(const char *arg);
int MaxStackDepth(void);

//           1         2         3         4         5         6         7
//  1234567890123456789012345678901234567890123456789012345678901234567890
int MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC(void);

int TriggerSegmentFaultException(void);
int TriggerTrapException(void);

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
int TestFunc10(void);
int TestFunc11(void);
int TestFunc12(void);
int TestFunc13(void);
int TestFunc14(void);
int TestFunc15(void);
int TestFunc16(void);
int TestFunc17(void);
int TestFunc18(void);
int TestFunc19(void);
int TestFunc20(void);
int TestFunc21(void);
int TestFunc22(void);
int TestFunc23(void);
int TestFunc24(void);
int TestFunc25(void);
int TestFunc26(void);
int TestFunc27(void);
int TestFunc28(void);
int TestFunc29(void);
int TestFunc30(void);
int TestFunc31(void);
int TestFunc32(void);
int TestFunc33(void);
int TestFunc34(void);
int TestFunc35(void);
int TestFunc36(void);
int TestFunc37(void);
int TestFunc38(void);
int TestFunc39(void);
int TestFunc40(void);
int TestFunc41(void);
int TestFunc42(void);
int TestFunc43(void);
int TestFunc44(void);
int TestFunc45(void);
int TestFunc46(void);
int TestFunc47(void);
int TestFunc48(void);
int TestFunc49(void);
int TestFunc50(void);
int TestFunc51(void);
int TestFunc52(void);
int TestFunc53(void);
int TestFunc54(void);
int TestFunc55(void);
int TestFunc56(void);
int TestFunc57(void);
int TestFunc58(void);
int TestFunc59(void);
int TestFunc60(void);
int TestFunc61(void);
int TestFunc62(void);
int TestFunc63(void);
int TestFunc64(void);
int TestFunc65(void);
int TestFunc66(void);
int TestFunc67(void);
int TestFunc68(void);
int TestFunc69(void);
int TestFunc70(void);

#endif // DFX_CRASHER_H
