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

#include "dfx_thread.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ptrace.h>
#include <sys/wait.h>

#include <securec.h>

#include "dfx_define.h"
#include "dfx_frames.h"

#include <hilog_base/log_base.h>
#include "dfx_regs.h"
#include "dfx_util.h"

BOOL InitThread(DfxThread **thread, pid_t pid, pid_t tid)
{
    *thread = (DfxThread *)calloc(1, sizeof(DfxThread));
    if (*thread == NULL) {
        HILOG_BASE_WARN(LOG_CORE, "Fail to alloc thread struct");
        return FALSE;
    }

    (*thread)->pid = pid;
    (*thread)->tid = tid;
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/comm", tid) <= 0) {
        return FALSE;
    }

    char buf[NAME_LEN] = {0};
    ReadStringFromFile(path, buf, sizeof(buf));
    (*thread)->threadName = TrimAndDupStr(buf);
#ifndef DFX_LOCAL_UNWIND
    if (ptrace(PTRACE_ATTACH, tid, NULL, NULL) != 0) {
        HILOG_BASE_WARN(LOG_CORE, "Fail to attach thread(%{public}d), errno=%{public}s", tid, strerror(errno));
        return FALSE;
    }

    errno = 0;
    while (waitpid(tid, NULL, __WALL) < 0) {
        if (EINTR != errno) {
            ptrace(PTRACE_DETACH, tid, NULL, NULL);
            HILOG_BASE_WARN(LOG_CORE, "Fail to wait thread(%{public}d) attached, errno=%{public}s",
                tid, strerror(errno));
            return FALSE;
        }
        errno = 0;
    }
#endif
    return TRUE;
}

BOOL InitThreadByContext(DfxThread **thread, pid_t pid, pid_t tid, ucontext_t *context)
{
    if (!InitThread(thread, pid, tid)) {
        HILOG_BASE_WARN(LOG_CORE, "Fail to init thread(%{public}d).", tid);
        return FALSE;
    }

    if (!InitRegsFromUcontext(&((*thread)->regs), context)) {
        HILOG_BASE_WARN(LOG_CORE, "Fail to load thread(%{public}d) context.", tid);
        return FALSE;
    }

    return TRUE;
}

DfxFrame *GetAvaliableFrame(DfxThread *thread)
{
    if (thread->head == NULL) {
        return CreateNewFrame(&(thread->head));
    }

    DfxFramesNode *curNode = thread->head;
    while (curNode != NULL && curNode->next != NULL) {
        curNode = curNode->next;
    }
    return CreateNewFrame(&(curNode->next));
}

void SkipFramesInSignalHandler(DfxThread *thread)
{
    if (thread == NULL) {
        return;
    }

    if (thread->regs == NULL) {
        return;
    }

    DfxFramesNode *curNode = thread->head;
    while (curNode != NULL) {
        DfxFrame *frame = curNode->frame;
        if ((frame != NULL) && (thread->regs->r[REG_PC_NUM] == frame->pc)) {
            thread->head = curNode;
            break;
        }
        curNode = curNode->next;
    }

    size_t index = 0;
    while (curNode != NULL) {
        DfxFrame *frame = curNode->frame;
        if ((frame != NULL)) {
            frame->index = index;
            index++;
        }
        curNode = curNode->next;
    }
}

void DestroyThread(DfxThread *thread)
{
    if (thread == NULL) {
        return;
    }
#ifndef DFX_LOCAL_UNWIND
    ptrace(PTRACE_DETACH, thread->tid, NULL, NULL);
#endif
    free(thread->threadName);
    thread->threadName = NULL;
    free(thread);
}

void PrintThread(const DfxThread *thread, int32_t fd)
{
    if (thread == NULL) {
        return;
    }

    PrintRegs(thread->regs, fd);
    PrintFrames(thread->head, fd);
}
