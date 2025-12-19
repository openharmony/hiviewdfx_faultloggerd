/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <thread>

#include "directory_ex.h"
#include "faultloggerd_test.h"
#include "faultloggerd_test_server.h"

#define private public
#include "fault_logger_daemon.h"
#include "smart_fd.h"
#include "temp_file_manager.h"

namespace OHOS {
namespace HiviewDFX {
using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace {
constexpr auto CPP_CRASH_FILE = "cppcrash";
constexpr auto JS_HEAP = "jsheap";
constexpr auto LEAK_STACK = "leakstack";
constexpr auto OTHER_FILE = "others";

TempFileManager& tempFileManager = FaultLoggerDaemon::GetInstance().tempFileManager_;

string GetFileName(const string &fileNamePrefix, uint32_t time)
{
    string fileName = TEST_TEMP_FILE_PATH;
    fileName += fileNamePrefix;
    fileName += "-";
    fileName += std::to_string(getpid());
    fileName += "-";
    constexpr int32_t secondsToMilliseconds = 1000;
    fileName += std::to_string(chrono::duration_cast<chrono::milliseconds>\
(chrono::system_clock::now().time_since_epoch()).count() - time * secondsToMilliseconds);
    return fileName;
}

bool IsFileExist(const string &fileName)
{
    error_code errorCode;
    return filesystem::exists(fileName, errorCode);
}

SmartFd CreateTestFile(const string& fileName, uint32_t fileSize = 1)
{
    GTEST_LOG_(INFO) << "Create test file: " << fileName;
    SmartFd fd{static_cast<int>(OHOS_TEMP_FAILURE_RETRY(open(fileName.c_str(),
        O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP)))};
    if (!fd|| fileSize == 0) {
        return fd;
    }
    constexpr int oneKb = 1024;
    char testContent[oneKb] = "";
    for (uint32_t i = 0; i < fileSize; ++i) {
        write(fd.GetFd(), testContent, oneKb);
    }
    return fd;
}
}

class TempFileManagerTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    void TearDown() override;
};

void TempFileManagerTest::TearDown()
{
    ClearTempFiles();
}

void TempFileManagerTest::SetUpTestCase()
{
    FaultLoggerdTestServer::GetInstance();
    ClearTempFiles();
}

/**
 * @tc.name: InvalidTempFileTest01
 * @tc.desc: delete the file, if the file is not configured.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, InvalidTempFileTest01, TestSize.Level2)
{
    string testFileName = GetFileName("testFiles", 0);
    ASSERT_TRUE(CreateTestFile(testFileName));
    this_thread::sleep_for(chrono::milliseconds(100));
    error_code errorCode;
    ASSERT_FALSE(IsFileExist(testFileName));
}

/**
 * @tc.name: OverTimeFileDeleteTest01
 * @tc.desc: remove all timeout files when new file created, if the count of the temporary files exceeds the limit.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverTimeFileDeleteTest01, TestSize.Level2)
{
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 66)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 65)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 64)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 63)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 62)));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 5);

    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 61)));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 0);
}

/**
 * @tc.name: OverTimeFileDeleteTest02
 * @tc.desc: only remove the timeout files when new file created, if the count of the temporary files exceeds the limit.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverTimeFileDeleteTest02, TestSize.Level2)
{
    string oldFilePath1 = GetFileName(CPP_CRASH_FILE, 66);
    ASSERT_TRUE(CreateTestFile(oldFilePath1));
    string oldFilePath2 = GetFileName(CPP_CRASH_FILE, 65);
    ASSERT_TRUE(CreateTestFile(oldFilePath2));
    string oldFilePath3 = GetFileName(CPP_CRASH_FILE, 64);
    ASSERT_TRUE(CreateTestFile(oldFilePath3));

    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 50)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 49)));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 5);

    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 48)));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 3);
    ASSERT_FALSE(IsFileExist(oldFilePath1));
    ASSERT_FALSE(IsFileExist(oldFilePath2));
    ASSERT_FALSE(IsFileExist(oldFilePath3));
}

/**
 * @tc.name: OverTimeFileDeleteTest03
 * @tc.desc: Delete the file as soon as it expires.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverTimeFileDeleteTest03, TestSize.Level2)
{
    ASSERT_TRUE(CreateTestFile(GetFileName(JS_HEAP, 0)));
    ASSERT_TRUE(CreateTestFile(GetFileName(JS_HEAP, 1)));
    ASSERT_EQ(CountTempFiles(), 2);
    this_thread::sleep_for(chrono::milliseconds(3100));
    ASSERT_EQ(CountTempFiles(), 0);
}

/**
 * @tc.name: OverMaxFileCountTest01
 * @tc.desc: remove the oldest files until the file count reached the keep file count,
 * if the count of the temporary files exceeds the max limit.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverMaxFileCountTest01, TestSize.Level2)
{
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 48)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 47)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 46)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 45)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 44)));
    string oldFilePath1 = GetFileName(CPP_CRASH_FILE, 50);
    ASSERT_TRUE(CreateTestFile(oldFilePath1));
    string oldFilePath2 = GetFileName(CPP_CRASH_FILE, 49);
    ASSERT_TRUE(CreateTestFile(oldFilePath2));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 7);

    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 43)));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 5);
    ASSERT_FALSE(IsFileExist(oldFilePath1));
    ASSERT_FALSE(IsFileExist(oldFilePath2));
}

/**
 * @tc.name: OverMaxFileCountTest02
 * @tc.desc: Delete the oldest file when the keepFileCount is configured as -1.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverMaxFileCountTest02, TestSize.Level2)
{
    ASSERT_TRUE(CreateTestFile(GetFileName(LEAK_STACK, 48)));
    string oldFilePath1 = GetFileName(LEAK_STACK, 49);
    ASSERT_TRUE(CreateTestFile(oldFilePath1));
    ASSERT_EQ(CountTempFiles(), 2);
    ASSERT_TRUE(CreateTestFile(GetFileName(LEAK_STACK, 46)));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_EQ(CountTempFiles(), 2);
    ASSERT_FALSE(IsFileExist(oldFilePath1));
}

/**
 * @tc.name: OverFileSizeFileTest01
 * @tc.desc: resize the file, if the size of the file exceeds the threshold.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverFileSizeFileTest01, TestSize.Level2)
{
    string testFileName = GetFileName(CPP_CRASH_FILE, 50);
    ASSERT_TRUE(CreateTestFile(testFileName, 6));
    this_thread::sleep_for(chrono::milliseconds(100));
    error_code errorCode;
    uint64_t fileSize = filesystem::file_size(testFileName, errorCode);
    ASSERT_EQ(fileSize, 5 << 10); // 5KB
}

/**
 * @tc.name: OverFileSizeFileTest02
 * @tc.desc: delete the file, if the size of the file exceeds the threshold.
 * @tc.type: FUNC
 */
HWTEST_F(TempFileManagerTest, OverFileSizeFileTest02, TestSize.Level2)
{
    string testOverLimitFile = GetFileName(JS_HEAP, 0);
    ASSERT_TRUE(CreateTestFile(testOverLimitFile, 6));
    string testEmptyFile = GetFileName(JS_HEAP, 1);
    ASSERT_TRUE(CreateTestFile(testEmptyFile, 0));
    this_thread::sleep_for(chrono::milliseconds(100));
    ASSERT_FALSE(IsFileExist(testOverLimitFile));
    ASSERT_FALSE(IsFileExist(testEmptyFile));
}

class ScanCurrentFilesTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void TearDown() override;
};

void ScanCurrentFilesTest::TearDown()
{
    ClearTempFiles();
}

void ScanCurrentFilesTest::SetUpTestCase()
{
    FaultLoggerdTestServer::GetInstance();
    ClearTempFiles();
    tempFileManager.SetFileEventMask(0xffffffff);
}

void ScanCurrentFilesTest::TearDownTestCase()
{
    tempFileManager.SetFileEventMask(0);
}

/**
 * @tc.name: ScanCurrentFilesOnStartTest01
 * @tc.desc: Delete js files immediately when the file size exceeds the limit.
 * @tc.type: FUNC
 */
HWTEST_F(ScanCurrentFilesTest, ScanCurrentFilesOnStartTest01, TestSize.Level2)
{
    ASSERT_TRUE(CreateTestFile(GetFileName(JS_HEAP, 0), 5));
    ASSERT_TRUE(CreateTestFile(GetFileName(JS_HEAP, 1), 5));
    ASSERT_TRUE(CreateTestFile(GetFileName(JS_HEAP, 2), 5));
    tempFileManager.ScanTempFilesOnStart();
    ASSERT_EQ(tempFileManager.GetTargetFileCount(FaultLoggerType::JS_HEAP_SNAPSHOT), 0);
}

/**
 * @tc.name: ScanCurrentFilesOnStartTest02
 * @tc.desc: Delete js files in 3 second when the file size not exceeds the limit.
 * @tc.type: FUNC
 */
HWTEST_F(ScanCurrentFilesTest, ScanCurrentFilesOnStartTest02, TestSize.Level2)
{
    string oldFilePath1 = GetFileName(JS_HEAP, 0);
    ASSERT_TRUE(CreateTestFile(oldFilePath1));
    string oldFilePath2 = GetFileName(JS_HEAP, 1);
    ASSERT_TRUE(CreateTestFile(oldFilePath2));
    FaultLoggerdTestServer::AddTask(TestThreadEnum::HELPER, []() {
        tempFileManager.ScanTempFilesOnStart();
    });
    this_thread::sleep_for(chrono::milliseconds(1000));
    ASSERT_EQ(tempFileManager.GetTargetFileCount(FaultLoggerType::JS_HEAP_SNAPSHOT), 2);
    this_thread::sleep_for(chrono::milliseconds(2500));
    ASSERT_FALSE(IsFileExist(oldFilePath1));
    ASSERT_FALSE(IsFileExist(oldFilePath2));
    tempFileManager.fileCounts_.clear();
}

/**
 * @tc.name: ScanCurrentFilesOnStartTest03
 * @tc.desc: Delete the oldest file when the file count exceeds the max file count.
 * @tc.type: FUNC
 */
HWTEST_F(ScanCurrentFilesTest, ScanCurrentFilesOnStartTest03, TestSize.Level2)
{
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 1)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 2)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 3)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 4)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 5)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 6)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 7)));
    ASSERT_TRUE(CreateTestFile(GetFileName(CPP_CRASH_FILE, 8)));
    tempFileManager.ScanTempFilesOnStart();
    ASSERT_EQ(tempFileManager.GetTargetFileCount(FaultLoggerType::CPP_CRASH), 5);
    tempFileManager.fileCounts_.clear();
}

/**
 * @tc.name: ScanCurrentFilesOnStartTest04
 * @tc.desc: Delete file immediately which is not configured.
 * @tc.type: FUNC
 */
HWTEST_F(ScanCurrentFilesTest, ScanCurrentFilesOnStartTest04, TestSize.Level2)
{
    string testFile = GetFileName(OTHER_FILE, 1);
    ASSERT_TRUE(CreateTestFile(OTHER_FILE));
    tempFileManager.ScanTempFilesOnStart();
    ASSERT_FALSE(IsFileExist(testFile));
}
}
}
