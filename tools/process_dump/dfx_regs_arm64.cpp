/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains arm64 reg module. */

#include <securec.h>

#include "dfx_regs.h"

#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
enum RegisterSeqNum {
    REG_0,
    REG_1,
    REG_2,
    REG_3,
    REG_4,
    REG_5,
    REG_6,
    REG_7,
    REG_8,
    REG_9,
    REG_10,
    REG_11,
    REG_12,
    REG_13,
    REG_14,
    REG_15,
    REG_16,
    REG_17,
    REG_18,
    REG_19,
    REG_20,
    REG_21,
    REG_22,
    REG_23,
    REG_24,
    REG_25,
    REG_26,
    REG_27,
    REG_28,
    REG_29,
    REG_30,
    REG_31,
    REG_32
};

DfxRegsArm64::DfxRegsArm64(const ucontext_t &context)
{
    DfxLogDebug("Enter %s.", __func__);
    std::vector<uintptr_t> regs {};
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_0]));   // 0:x0
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_1]));   // 1:x1
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_2]));   // 2:x2
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_3]));   // 3:x3
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_4]));   // 4:x4
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_5]));   // 5:x5
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_6]));   // 6:x6
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_7]));   // 7:x7
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_8]));   // 8:x8
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_9]));   // 9:x9
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_10])); // 10:x10
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_11])); // 11:x11
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_12])); // 12:x12
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_13])); // 13:x13
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_14])); // 14:x14
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_15])); // 15:x15
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_16])); // 16:x16
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_17])); // 17:x17
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_18])); // 18:x18
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_19])); // 19:x19
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_20])); // 20:x20
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_21])); // 21:x21
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_22])); // 22:x22
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_23])); // 23:x23
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_24])); // 24:x24
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_25])); // 25:x25
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_26])); // 26:x26
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_27])); // 27:x27
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_28])); // 28:x28
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_29])); // 29:x29
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_30])); // 30:lr
    regs.push_back(uintptr_t(context.uc_mcontext.sp));       // 31:sp
    regs.push_back(uintptr_t(context.uc_mcontext.pc));       // 32:pc

    SetRegs(regs);
    DfxLogDebug("lr:%016lx sp:%016lx pc:%016lx\n", regs[REG_30], regs[REG_31], regs[REG_32]);
    DfxLogDebug("Exit %s.", __func__);
}

std::string DfxRegsArm64::PrintRegs() const
{
    DfxLogDebug("Enter %s.", __func__);

    std::string regString = "";
    char buf[LOG_BUF_LEN] = {0};

    regString = regString + "Registers:\n";

    std::vector<uintptr_t> regs = GetRegsData();

    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x0:%016lx x1:%016lx x2:%016lx x3:%016lx\n", \
        regs[REG_0], regs[REG_1], regs[REG_2], regs[REG_3]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x4:%016lx x5:%016lx x6:%016lx x7:%016lx\n", \
        regs[REG_4], regs[REG_5], regs[REG_6], regs[REG_7]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x8:%016lx x9:%016lx x10:%016lx x11:%016lx\n", \
        regs[REG_8], regs[REG_9], regs[REG_10], regs[REG_11]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x12:%016lx x13:%016lx x14:%016lx x15:%016lx\n", \
        regs[REG_12], regs[REG_13], regs[REG_14], regs[REG_15]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

        regs[REG_18], regs[REG_19]);
    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x16:%016lx x17:%016lx x18:%016lx x19:%016lx\n", \
        regs[REG_16], regs[REG_17], regs[REG_18], regs[REG_19]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x20:%016lx x21:%016lx x22:%016lx x23:%016lx\n", \
        regs[REG_20], regs[REG_21], regs[REG_22], regs[REG_23]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x24:%016lx x25:%016lx x26:%016lx x27:%016lx\n", \
        regs[REG_24], regs[REG_25], regs[REG_26], regs[REG_27]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "x28:%016lx x29:%016lx\n", regs[REG_28], regs[REG_29]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "lr:%016lx sp:%016lx pc:%016lx\n", \
        regs[REG_30], regs[REG_31], regs[REG_32]);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    regString = regString + std::string(buf);
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: memset_s failed, line: %d.", __func__, __LINE__);
    }

    DfxLogDebug("Exit %s.", __func__);
    return regString;
}
} // namespace HiviewDFX
} // namespace OHOS
