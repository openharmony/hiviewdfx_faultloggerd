/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "dfx_elf.h"
#include "dfx_maps.h"
#include <gtest/gtest.h>
#include <memory>
#include <sys/types.h>

#include "file_util.h"

using namespace OHOS::HiviewDFX;
using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace {
const string INVALID_MAP_NAME = "/system/lib64/init/libinit_context111.z.so";
const string ADLT_MAP_NAME = "/system/lib/libadlt_app.so";
#ifdef __arm__
const string MAPS_FILE = "/data/test/resource/testdata/testmaps_32";
const string VALID_MAP_NAME = "/system/lib/init/libinit_context.z.so";
const string VALID_MAP_ITEM = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so";
const string INVALID_MAP_ITEM = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context111.z.so";
const string VDSO_MAP_ITEM = "f7c21000-f7c22000 r-xp 00000000 00:00 0                                  [vdso]";
const string ARK_HAP_MAP_NAME = "/system/app/SceneBoard/SceneBoard.hap";
const string ARK_CODE_MAP_NAME = "[anon:ArkTS Code:libdialog.z.so/Dialog.js]";
#define ADLT_MAP_LOAD_BASE 0xa0400000
#else
const string MAPS_FILE = "/data/test/resource/testdata/testmaps_64";
const string VALID_MAP_NAME = "/system/lib64/init/libinit_context.z.so";
const string VALID_MAP_ITEM = "7f0ab40000-7f0ab41000 r--p 00000000 b3:07 1882 /system/lib64/init/libinit_context.z.so";
const string INVALID_MAP_ITEM = "7f0ab40000-7f0ab41000 r--p 00000000 b3:07 1882 \
    /system/lib64/init/libinit_context111.z.so";
const string VDSO_MAP_ITEM = "7f8b9df000-7f8b9e0000 r-xp 00000000 00:00 0                              [vdso]";
const string ARK_HAP_MAP_NAME = "/system/app/SceneBoard/SceneBoard.hap";
const string ARK_CODE_MAP_NAME = "[anon:ArkTS Code:libdialog.z.so/Dialog.js]";
#define ADLT_MAP_LOAD_BASE 0x5aa0400000
#endif
}

class MapsTest : public testing::Test {
public:
    void SetUp() { maps_ = DfxMaps::Create(getpid(), MAPS_FILE); }

public:
    std::shared_ptr<DfxMaps> maps_;
};

namespace {

/**
 * @tc.name: FindMapByAddrTest001
 * @tc.desc: test FindMapByAddr functions
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByAddrTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByAddrTest001: start.";
    maps_->Sort(true);
#ifdef __arm__
    uintptr_t validAddr = 0xf6d80000;
#else
    uintptr_t validAddr = 0x7f8b8f3001;
#endif
    const uintptr_t invalidAddr = 0xffffffff;
    std::shared_ptr<DfxMap> map = nullptr;
    EXPECT_EQ(true, maps_->FindMapByAddr(validAddr, map));
    EXPECT_EQ(false, maps_->FindMapByAddr(invalidAddr, map));
    GTEST_LOG_(INFO) << "FindMapByAddrTest001: end.";
}

/**
 * @tc.name: FindMapByFileInfoTest001
 * @tc.desc: test FindMapByFileInfo functions
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByFileInfoTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest001: start.";
    std::shared_ptr<DfxMap> map = nullptr;
    EXPECT_EQ(true, maps_->FindMapByFileInfo(VALID_MAP_NAME, 0, map));
    EXPECT_EQ(false, maps_->FindMapByFileInfo(INVALID_MAP_NAME, 0, map));
    const uint64_t invalidOffset = 0xffffffff;
    EXPECT_EQ(false, maps_->FindMapByFileInfo(VALID_MAP_NAME, invalidOffset, map));
    EXPECT_EQ(false, maps_->FindMapByFileInfo(INVALID_MAP_NAME, invalidOffset, map));
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest001: end.";
}

/**
 * @tc.name: FindMapsByNameTest001
 * @tc.desc: test FindMapsByName functions
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapsByNameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapsByNameTest001: start.";
    std::vector<std::shared_ptr<DfxMap>> mapsV;
    EXPECT_EQ(true, maps_->FindMapsByName(VALID_MAP_NAME, mapsV));
    mapsV.clear();
    EXPECT_EQ(false, maps_->FindMapsByName(INVALID_MAP_NAME, mapsV));
    GTEST_LOG_(INFO) << "FindMapsByNameTest001: end.";
}

/**
 * @tc.name: IsArkNameTest001
 * @tc.desc: test IsArkExecutable functions
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, IsArkNameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsArkNameTest001: start.";
    std::shared_ptr<DfxMap> map = nullptr;
    map = std::make_shared<DfxMap>(0, 0, 0, "1", "anon:ArkTS Code");
    EXPECT_EQ(false, map->IsArkExecutable());
    map = std::make_shared<DfxMap>(0, 0, 0, "1", "/dev/zero");
    EXPECT_EQ(false, map->IsArkExecutable());
    map = std::make_shared<DfxMap>(0, 0, 0, 4, "[anon:ArkTS Code]");
    EXPECT_EQ(true, map->IsArkExecutable());
    GTEST_LOG_(INFO) << "IsArkNameTest001: end.";
}

/**
 * @tc.name: IsJsvmNameTest001
 * @tc.desc: test IsJsvmExecutable function
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, IsJsvmNameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsArkNameTest001: start.";
    std::shared_ptr<DfxMap> map = nullptr;
    map = std::make_shared<DfxMap>(0, 0, 0, "1", "");
    EXPECT_FALSE(map->IsJsvmExecutable());
    map = std::make_shared<DfxMap>(0, 0, 0, "1", "[anon:JSVM_JIT]");
    EXPECT_TRUE(map->IsJsvmExecutable());
    map = std::make_shared<DfxMap>(0, 0, 0, PROT_EXEC, "libv8_shared.so");
    EXPECT_TRUE(map->IsJsvmExecutable());
    map = std::make_shared<DfxMap>(0, 0, 0, PROT_READ, "libv8_shared.so");
    EXPECT_FALSE(map->IsJsvmExecutable());
    GTEST_LOG_(INFO) << "IsArkNameTest001: end.";
}

/**
 * @tc.name: GetRelPcTest
 * @tc.desc: test getRelPc no elf
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, GetRelPcTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetRelPcTest: start.";
#ifdef __arm__
    uint64_t pc = 0xf6d83001;
    const uint64_t invalidOffset = 0x1001;
#else
    uint64_t pc = 0x7f0ab40016;
    const uint64_t invalidOffset = 0x16;
#endif
    shared_ptr<DfxMap> map = DfxMap::Create(INVALID_MAP_ITEM);
    EXPECT_EQ(true, ((map->GetElf() == nullptr) && (map->GetRelPc(pc) == invalidOffset)));
    EXPECT_EQ(true, ((map->GetElfLongLong() == nullptr) && (map->GetRelPc(pc) == invalidOffset)));
    GTEST_LOG_(INFO) << "GetRelPcTest: end.";
}

/**
 * @tc.name: ToStringTest
 * @tc.desc: test ToString
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, ToStringTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ToStringTest: start.";
    shared_ptr<DfxMap> map = DfxMap::Create(VALID_MAP_ITEM);
    GTEST_LOG_(INFO) << map->ToString();
    EXPECT_EQ(true, sizeof(map->ToString()) != 0);
    GTEST_LOG_(INFO) << "ToStringTest: end.";
}

/**
 * @tc.name: CreateMapsTest
 * @tc.desc: test create maps by pid
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, CreateMapsTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CreateMapsTest: start.";
    std::shared_ptr<DfxMaps> dfxMaps = DfxMaps::Create(getpid());
    ASSERT_TRUE(dfxMaps != nullptr);
    auto maps = dfxMaps->GetMaps();
    for (auto map : maps) {
        std::cout << map->ToString();
    }
    GTEST_LOG_(INFO) << "CreateMapsTest: end.";
}

/**
 * @tc.name: AdltMapLoadBaseTest
 * @tc.desc: test adlt map loadbase
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, AdltMapLoadBaseTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AdltMapLoadBaseTest: start.";
    std::vector<std::shared_ptr<DfxMap>> mapsV;
    EXPECT_EQ(true, maps_->FindMapsByName(ADLT_MAP_NAME, mapsV));
    for (auto map : mapsV) {
        EXPECT_EQ(ADLT_MAP_LOAD_BASE, map->GetAdltLoadBase());
    }
    GTEST_LOG_(INFO) << "invalid pid or path test.";
    std::string invalidPath = "";
    ASSERT_TRUE(DfxMaps::Create(-1, invalidPath) == nullptr);
    ASSERT_TRUE(DfxMaps::Create(getpid(), invalidPath) == nullptr);
    invalidPath = "/data/xxxx";
    ASSERT_TRUE(DfxMaps::Create(getpid(), invalidPath) == nullptr);
    GTEST_LOG_(INFO) << "AdltMapLoadBaseTest: end.";
}

/**
 * @tc.name: AdltMapIndexTest
 * @tc.desc: test adlt map index
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, AdltMapIndexTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AdltMapIndexTest: start.";
    std::vector<std::shared_ptr<DfxMap>> mapsV;
    ASSERT_TRUE(maps_->adltMapIndex_ >= 0);
    ASSERT_TRUE(maps_->adltMapIndex_ < maps_->maps_.size());
    EXPECT_EQ(true, maps_->FindMapsByName(ADLT_MAP_NAME, mapsV));
    for (auto map : mapsV) {
        EXPECT_EQ(maps_->adltMapIndex_, maps_->FindMapIndexByAddr(map->begin));
    }

    GTEST_LOG_(INFO) << "AdltMapIndexTest: end.";
}

/**
 * @tc.name: GetStackRangeTest
 * @tc.desc: test GetStackRange
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, GetStackRangeTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetStackRangeTest: start.";
    uintptr_t bottom, top;
    ASSERT_TRUE(maps_->GetStackRange(bottom, top));
#ifdef __arm__
    EXPECT_EQ(bottom, 0xff860000);
    EXPECT_EQ(top, 0xff881000);
#else
    EXPECT_EQ(bottom, 0x7fe37db000);
    EXPECT_EQ(top, 0x7fe37fc000);
#endif
    GTEST_LOG_(INFO) << "GetStackRangeTest: end.";
}

/**
 * @tc.name: IsArkExecutedMapTest
 * @tc.desc: test IsArkExecutedMap
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, IsArkExecutedMapTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsArkExecutedMapTest: start.";
    uintptr_t addr;
#ifdef __arm__
    addr = 0xffff2001;
#else
    addr = 0x7fe37fd001;
#endif
    ASSERT_TRUE(maps_->IsArkExecutedMap(addr));
#ifdef __arm__
    addr = 0xffff1001;
#else
    addr = 0x7fe37fc001;
#endif
    ASSERT_FALSE(maps_->IsArkExecutedMap(addr));
    addr = 0x0;
    ASSERT_FALSE(maps_->IsArkExecutedMap(addr));
    maps_->Sort(false);
    GTEST_LOG_(INFO) << "IsArkExecutedMapTest: end.";
}

/**
 * @tc.name: IsVdsoMapTest
 * @tc.desc: test IsVdsoMap
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, IsVdsoMapTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsVdsoMapTest: start.";
    shared_ptr<DfxMap> map = DfxMap::Create(VDSO_MAP_ITEM);
    ASSERT_TRUE(map->IsVdsoMap());
    GTEST_LOG_(INFO) << "IsVdsoMapTest: end.";
}

/**
 * @tc.name: IsLegalMapItemTest
 * @tc.desc: test IsLegalMapItem, IsArkHapMapItem, IsArkCodeMapItem
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, IsLegalMapItemTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsLegalMapItemTest: start.";
    ASSERT_TRUE(DfxMaps::IsArkHapMapItem(ARK_HAP_MAP_NAME));
    ASSERT_TRUE(DfxMaps::IsArkCodeMapItem(ARK_CODE_MAP_NAME));
    ASSERT_TRUE(DfxMaps::IsLegalMapItem(ARK_CODE_MAP_NAME));
    ASSERT_TRUE(DfxMaps::IsLegalMapItem("[anon:JSVM_JIT]"));
    ASSERT_TRUE(DfxMaps::IsLegalMapItem("[anon:ARKWEB_JIT]"));
    ASSERT_TRUE(DfxMaps::IsLegalMapItem("[anon:v8]"));
    GTEST_LOG_(INFO) << "IsLegalMapItemTest: end.";
}

/**
 * @tc.name: CreateByBuffer001
 * @tc.desc: test create by buffer
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, CreateByBuffer001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CreateByBuffer001: start.";
    std::string mapsPath = "/proc/self/maps";
    std::string content;
    std::string bundleName = "root";
    EXPECT_TRUE(DfxMaps::CreateByBuffer(bundleName, content) == nullptr);
    LoadStringFromFile(mapsPath, content);
    EXPECT_TRUE(DfxMaps::CreateByBuffer(bundleName, content) != nullptr);
    GTEST_LOG_(INFO) << "CreateByBuffer001: end.";
}

/**
 * @tc.name: CreateByBuffer002
 * @tc.desc: test create by buffer common hap
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, CreateByBuffer002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CreateByBuffer002: start.";
    std::string content =  "7ef9f8b000-7efa1f1000 r--s 038ca000 1ff:02 62203520                      "
                            "/system/app/SceneBoard/SceneBoard.hap\nOpenFiles:\n"
                            "0->/dev/null native object of unknown type 0\n"
                            "1->/dev/null native object of unknown type 0\n"
                            "2->/dev/null native object of unknown type 0\n"
                            "3->socket:[1293] native object of unknown type 218115328\n"
                            "4->anon_inode:[eventpoll] FILE* 4\n"
                            "5->socket:[1295] native object of unknown type 0\n";
    std::string bundleName = "root";
    EXPECT_TRUE(DfxMaps::CreateByBuffer(bundleName, content) != nullptr);
    EXPECT_FALSE(DfxMaps::IsArkWebProc());
    GTEST_LOG_(INFO) << "CreateByBuffer002: end.";
}

/**
 * @tc.name: CreateByBuffer003
 * @tc.desc: test create by buffer contain arkweb
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, CreateByBuffer003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CreateByBuffer003: start.";
    std::string content =  "5a0f486000-5a10373000 r--p 00000000 "
                           "/data/app/el1/bundle/public/com.hmos.arkweb/libs/arm64/libarkweb_engine.so\n"
                            "5a19631000-5a19e87000 r--p 00000000 "
                            "/data/service/el1/public/for-all-app/shared_relro/libwebviewchromium64.relro\n";
    std::string bundleName = "root";
    EXPECT_TRUE(DfxMaps::CreateByBuffer(bundleName, content) == nullptr);
    EXPECT_FALSE(DfxMaps::IsArkWebProc());

    content = "5a0f486000-5a10373000 r--p 00000000 104:4f 8320                          "
              "/data/storage/el1/bundle/arkwebcore/libs/arm64/libarkweb_engine.so\n"
              "5a3e1b7000-5a3e1c6000 rw-p 00174000 104:4f 8321         ss                 "
              "/data/storage/el1/bundle/arkwebcore/libs/arm64/libarkweb_render.so\n";

    EXPECT_TRUE(DfxMaps::CreateByBuffer(bundleName, content) != nullptr);
    EXPECT_TRUE(DfxMaps::IsArkWebProc());
    GTEST_LOG_(INFO) << "CreateByBuffer003: end.";
}

/**
 * @tc.name: MapParseTest001
 * @tc.desc: test parse map by string.
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, MapParseTest001, TestSize.Level2)
{
    DfxMap dfxMap;
    ASSERT_FALSE(dfxMap.Parse(nullptr, 0));
    constexpr auto testCase1 = "\t 7A0ab40000 ";
    ASSERT_FALSE(dfxMap.Parse(testCase1, strlen(testCase1)));
    constexpr size_t maxTestSize = 1024 * 4;
    constexpr size_t normalTestSize = 1024 / 2;
    ASSERT_FALSE(dfxMap.Parse(testCase1, maxTestSize));
    ASSERT_FALSE(dfxMap.Parse(testCase1, normalTestSize));
    constexpr auto testCase2 = "7f0ab40000*";
    ASSERT_FALSE(dfxMap.Parse(testCase2, strlen(testCase2)));
    constexpr auto testCase3 = "7f0ab40000-";
    ASSERT_FALSE(dfxMap.Parse(testCase3, strlen(testCase3)));
    constexpr auto testCase4 = "7f0ab40000-7f0ab41000";
    ASSERT_FALSE(dfxMap.Parse(testCase4, strlen(testCase4)));
    ASSERT_FALSE(dfxMap.Parse(testCase4, normalTestSize));
    constexpr auto testCase5 = "7f0ab40000-7f0ab41000 r---p";
    ASSERT_FALSE(dfxMap.Parse(testCase5, strlen(testCase5)));
    constexpr auto testCase6 = "7f0ab40000-7f0ab41000 ra-p\t";
    ASSERT_FALSE(dfxMap.Parse(testCase6, strlen(testCase6)));
    constexpr auto testCase7 = "7f0ab40000-7f0ab41000 r--p ";
    ASSERT_FALSE(dfxMap.Parse(testCase7, strlen(testCase7)));
    constexpr auto testCase8 = "7f0ab40000-7f0ab41000 r--p 00000b37";
    ASSERT_FALSE(dfxMap.Parse(testCase8, strlen(testCase8)));
    constexpr auto testCase9 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3";
    ASSERT_FALSE(dfxMap.Parse(testCase9, strlen(testCase9)));
    constexpr auto testCase10 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3-";
    ASSERT_FALSE(dfxMap.Parse(testCase10, strlen(testCase10)));
    constexpr auto testCase11 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3:";
    ASSERT_TRUE(dfxMap.Parse(testCase11, strlen(testCase11)));
    constexpr auto testCase12 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3:07 ";
    ASSERT_TRUE(dfxMap.Parse(testCase12, strlen(testCase12)));
    constexpr auto testCase13 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3:07 a";
    ASSERT_TRUE(dfxMap.Parse(testCase13, strlen(testCase13)));
    constexpr auto testCase14 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3:07 1882";
    ASSERT_TRUE(dfxMap.Parse(testCase14, strlen(testCase14)));
    constexpr auto testCase15 = "7f0ab40000-7f0ab41000 r--p 00000b37 b3:07 1882 /system/lib64/init/test.z.so";
    ASSERT_TRUE(dfxMap.Parse(testCase15, strlen(testCase15)));
    ASSERT_EQ(dfxMap.begin, 0x7f0ab40000);
    ASSERT_EQ(dfxMap.end, 0x7f0ab41000);
    ASSERT_EQ(dfxMap.perms, "r--p");
    ASSERT_EQ(dfxMap.offset, 0xb37);
    ASSERT_EQ(dfxMap.major, 0xb3);
    ASSERT_EQ(dfxMap.minor, 0x07);
    ASSERT_EQ(dfxMap.inode, 1882);
    ASSERT_EQ(dfxMap.name, "/system/lib64/init/test.z.so");
}
}
} // namespace HiviewDFX
} // namespace OHOS
