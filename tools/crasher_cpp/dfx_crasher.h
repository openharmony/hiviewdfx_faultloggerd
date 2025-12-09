/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_CRASHER_H
#define DFX_CRASHER_H

#include <cinttypes>
#include <string>
#include "dfx_test_func.h"

class DfxCrasher {
public:

    static DfxCrasher &GetInstance();
    ~DfxCrasher();

    static int RaiseAbort();
    static int RaiseBusError();
    static int RaiseFloatingPointException();
    static int RaiseIllegalInstructionException();
    static int RaiseSegmentFaultException();
    static int RaiseTrapException();
    static int IllegalInstructionException();
    static int SegmentFaultException();
    static int SetLastFatalMessage();
    static int Abort();
    static int Loop();
    static int TriggerPipeException();
    static int TriggerSocketException();

    void PrintUsage() const;

    static void* DoCrashInThread(void* inputArg);
    uint64_t DoActionOnSubThread(const char* arg) const;
    uint64_t ParseAndDoCrash(const char* arg) const;
    static int MaxStackDepth();
    static int MultiThreadCrash();
    static int ProgramCounterZero();
    static int StackOver64();
    static int StackTop();
    //           1         2         3         4         5         6         7
    //  1234567890123456789012345678901234567890123456789012345678901234567890
    static int MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC();
    static int TriggerSegmentFaultException();
    static int StackOverflow();
    static int Oom();
    static int TriggerTrapException();
    static int CrashInLambda();
    static int DoDumpCrash();
    static int TestExitHook();
    static int TestSigHook();
    static int StackCorruption();
    static int StackCorruption2();

    static int RecursiveFunc(int curLevel, int targetLevel, int midLevel);
    static int PrintFatalMessageInLibc();
    static int TestGetCrashObj();
    static int TestGetCrashObj63Kb();
    static int TestGetCrashObj64Kb();
    static int TestGetCrashObj65Kb();
    static int TestGetCrashObjMemory();
#ifndef is_ohos_lite
    static int CrashInFFRT(const std::string &debug);
    static int CrashInLibuvWork();
    static int CrashInLibuvTimer();
    static int CrashInLibuvWorkDone();
#endif
    static int TestDeadlock();

private:
    DfxCrasher();
    DfxCrasher(const DfxCrasher &) = delete;
    DfxCrasher &operator=(const DfxCrasher &) = delete;
};
int SleepThread(int threadID);
#endif // DFX_CRASHER_H
