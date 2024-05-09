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

#include "dfx_kernel_stack.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dfx_log.h"

#define LOGGER_GET_STACK    _IO(0xAB, 9)
namespace OHOS {
namespace HiviewDFX {
namespace {
// keep sync with defines in kernel/hicollie
const char* const BBOX_PATH = "/dev/bbox";
const int BUFF_STACK_SIZE = 20 * 1024;
const uint32_t MAGIC_NUM = 0x9517;
typedef struct HstackVal {
    uint32_t magic {0};
    pid_t pid {0};
    char hstackLogBuff[BUFF_STACK_SIZE] {0};
} HstackVal;
}
int DfxGetKernelStack(int32_t pid, std::string& kernelStack)
{
    auto kstackBuf = std::make_shared<HstackVal>();
    if (kstackBuf == nullptr) {
        DFXLOG_WARN("Failed create HstackVal, errno:%d", errno);
        return -1;
    }
    kstackBuf->pid = pid;
    kstackBuf->magic = MAGIC_NUM;
    int fd = open(BBOX_PATH, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        DFXLOG_WARN("Failed to open bbox, errno:%d", errno);
        return -1;
    }

    int ret = ioctl(fd, LOGGER_GET_STACK, kstackBuf.get());
    if (ret != 0) {
        DFXLOG_WARN("Failed to get kernel stack, errno:%d", errno);
    } else {
        kernelStack = std::string(kstackBuf->hstackLogBuff);
    }
    close(fd);
    return ret;
}
}
}