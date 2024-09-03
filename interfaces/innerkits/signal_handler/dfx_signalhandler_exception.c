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
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_exception.h"
#include "dfx_socket_request.h"
#include "errno.h"
#include "string.h"

#ifndef DFX_SIGNAL_LIBC
#include "dfx_log.h"
#else
#include "musl_log.h"
#endif

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
        DFXLOG_ERROR("Failed to create a socket, errno(%d).", errno);
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
                DFXLOG_ERROR("setsockopt SO_RCVTIMEO error, errno(%d).", errno);
                syscall(SYS_close, fd);
                fd = -1;
                break;
            }
        }
        struct sockaddr_un server;
        (void)memset(&server, 0, sizeof(server));
        server.sun_family = AF_LOCAL;
        (void)strncpy(server.sun_path, path, sizeof(server.sun_path) - 1);
        int len = sizeof(server.sun_family) + strlen(server.sun_path);
        int connected = OHOS_TEMP_FAILURE_RETRY(connect(fd, (struct sockaddr*)(&server), len));
        if (connected < 0) {
            DFXLOG_ERROR("Failed to connect to faultloggerd socket, errno = %d.", errno);
            syscall(SYS_close, fd);
            fd = -1;
            break;
        }
    } while (false);
    return fd;
}

static bool CheckReadResp(int fd)
{
    char controlBuffer[MAX_FUNC_NAME_LEN] = {0};
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, controlBuffer, sizeof(controlBuffer) - 1));
    if (nread != (ssize_t)(strlen(FAULTLOGGER_DAEMON_RESP))) {
        DFXLOG_ERROR("Failed to read expected length, nread: %zd, errno(%d).", nread, errno);
        return false;
    }
    return true;
}

int ReportException(struct CrashDumpException exception)
{
    struct FaultLoggerdRequest request;
    (void)memset(&request, 0, sizeof(struct FaultLoggerdRequest));
    request.clientType = (int32_t)REPORT_EXCEPTION_CLIENT;
    request.pid = exception.pid;
    request.uid = (uint32_t)(exception.uid);
    int ret = -1;
    int fd = ConnectSocket(FAULTLOGGERD_SOCKET_NAME, TIME_OUT); // connect timeout
    if (fd == -1) {
        DFXLOG_ERROR("Failed to connect socket.");
        return ret;
    }
    do {
        if (OHOS_TEMP_FAILURE_RETRY(write(fd, &request, sizeof(request))) != (long)sizeof(request)) {
            DFXLOG_ERROR("Failed to write request message to socket, errno(%d).", errno);
            break;
        }

        if (!CheckReadResp(fd)) {
            DFXLOG_ERROR("Failed to receive socket responces.");
            break;
        }

        if (OHOS_TEMP_FAILURE_RETRY(write(fd, &exception,
            sizeof(exception))) != (long)sizeof(exception)) {
            DFXLOG_ERROR("Failed to write exception message to socket, errno(%d).", errno);
            break;
        }

        ret = 0;
    } while (false);
    syscall(SYS_close, fd);
    return ret;
}
