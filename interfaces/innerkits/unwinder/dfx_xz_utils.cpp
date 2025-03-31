/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "dfx_xz_utils.h"
#include "dfx_trace_dlsym.h"
#include "7zCrc.h"
#include "Xz.h"
#include "XzCrc64.h"
#include <cstdlib>

namespace OHOS {
namespace HiviewDFX {

#define EXPAND_FACTOR 2

static void* XzAlloc(ISzAllocPtr, size_t size)
{
    return malloc(size);
}

static void XzFree(ISzAllocPtr, void *address)
{
    free(address);
}

bool XzDecompress(const uint8_t *src, size_t srcLen, std::shared_ptr<std::vector<uint8_t>> out)
{
    DFX_TRACE_SCOPED_DLSYM("XzDecompress");
    ISzAlloc alloc;
    CXzUnpacker state;
    alloc.Alloc = XzAlloc;
    alloc.Free = XzFree;
    XzUnpacker_Construct(&state, &alloc);
    CrcGenerateTable();
    Crc64GenerateTable();
    size_t srcOff = 0;
    size_t dstOff = 0;
    std::vector<uint8_t> dst(srcLen, ' ');
    ECoderStatus status = CODER_STATUS_NOT_FINISHED;
    while (status == CODER_STATUS_NOT_FINISHED) {
        dst.resize(dst.size() * EXPAND_FACTOR);
        size_t srcRemain = srcLen - srcOff;
        size_t dstRemain = dst.size() - dstOff;
        int res = XzUnpacker_Code(&state,
                                  reinterpret_cast<Byte*>(&dst[dstOff]), &dstRemain,
                                  reinterpret_cast<const Byte*>(&src[srcOff]), &srcRemain,
                                  true, CODER_FINISH_ANY, &status);
        if (res != SZ_OK) {
            XzUnpacker_Free(&state);
            return false;
        }
        srcOff += srcRemain;
        dstOff += dstRemain;
    }
    XzUnpacker_Free(&state);
    if (!XzUnpacker_IsStreamWasFinished(&state)) {
        return false;
    }
    dst.resize(dstOff);
    *out = std::move(dst);
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS
