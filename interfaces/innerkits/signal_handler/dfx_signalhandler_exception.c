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

#include "dfx_signalhandler_exception.h"

#include <stdio.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_exception.h"
#include "errno.h"
#include "string.h"
#include "dfx_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSignalHandlerException"
#endif

static const int TIME_OUT = 2;       /* seconds */
static const char FAULTLOGGERD_SOCKET_NAME[] = "/dev/unix/socket/faultloggerd.server";

static int ConnectSocket(const char* path, const int timeout)
{
    int fd = -1;
    if ((fd = syscall(SYS_socket, AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DFXLOGE("Failed to create a socket, errno(%{public}d).", errno);
        return -1;
    }

    do {
        if (timeout > 0) {
            struct timeval timev = {
                timeout,
                0
            };
            void* pTimev = &timev;
            if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_setsockopt, fd, SOL_SOCKET, SO_RCVTIMEO,
                (const void*)(pTimev), sizeof(timev))) != 0) {
                DFXLOGE("setsockopt SO_RCVTIMEO error, errno(%{public}d).", errno);
                syscall(SYS_close, fd);
                fd = -1;
                break;
            }
        }
        struct sockaddr_un server;
        (void)memset_s(&server, sizeof(server), 0, sizeof(server));
        server.sun_family = AF_LOCAL;
        if (strcpy_s(server.sun_path, sizeof(server.sun_path), path) != EOK) {
            DFXLOGE("server sun_path strcpy fail.");
        }
        int len = sizeof(server.sun_family) + strlen(server.sun_path);
        int connected = OHOS_TEMP_FAILURE_RETRY(connect(fd, (struct sockaddr*)(&server), len));
        if (connected < 0) {
            DFXLOGE("Failed to connect to faultloggerd socket, errno = %{public}d.", errno);
            syscall(SYS_close, fd);
            fd = -1;
            break;
        }
    } while (false);
    return fd;
}

int ReportException(struct CrashDumpException* exception)
{
    if (exception == NULL) {
        return -1;
    }
    exception->head.clientType = REPORT_EXCEPTION_CLIENT;
    exception->head.clientPid = getpid();
    int ret = -1;
    int fd = ConnectSocket(FAULTLOGGERD_SOCKET_NAME, TIME_OUT); // connect timeout
    if (fd == -1) {
        DFXLOGE("Failed to connect socket.");
        return ret;
    }
    do {
        if (OHOS_TEMP_FAILURE_RETRY(write(fd, exception, sizeof(struct CrashDumpException))) !=
            (long)sizeof(struct CrashDumpException)) {
            DFXLOGE("Failed to write request message to socket, errno(%{public}d).", errno);
            break;
        }
        ret = 0;
    } while (false);
    syscall(SYS_close, fd);
    return ret;
}
