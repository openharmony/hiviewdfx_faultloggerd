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

#ifndef DFX_FAULTLOGGERD_SOCKET_H
#define DFX_FAULTLOGGERD_SOCKET_H

#include <cinttypes>

#ifdef __cplusplus
extern "C" {
#endif
const char* const FAULTLOGGERD_SOCK_BASE_PATH = "/dev/unix/socket/";

#ifdef FAULTLOGGERD_TEST
const char* const SERVER_SOCKET_NAME = "test.faultloggerd.server";
const char* const SERVER_CRASH_SOCKET_NAME = "test.faultloggerd.crash.server";
const char* const SERVER_SDKDUMP_SOCKET_NAME = "test.faultloggerd.sdkdump.server";
#else
const char* const SERVER_SOCKET_NAME = "faultloggerd.server";
const char* const SERVER_CRASH_SOCKET_NAME = "faultloggerd.crash.server";
const char* const SERVER_SDKDUMP_SOCKET_NAME = "faultloggerd.sdkdump.server";
#endif

bool StartListen(int32_t& sockFd, const char* name, uint32_t listenCnt);
bool StartConnect(int32_t sockFd, const char* socketName, uint32_t timeout);
int32_t CreateSocketFd();
bool SendFileDescriptorToSocket(int32_t sockFd, const int32_t* fds, uint32_t nFds);
bool ReadFileDescriptorFromSocket(int32_t sockFd, int32_t* fds, uint32_t nFds);

bool SendMsgToSocket(int32_t sockFd, const void* data, uint32_t dataLength);
bool GetMsgFromSocket(int32_t sockFd, void* data, uint32_t dataLength);
#ifdef __cplusplus
}
#endif
#endif
