/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <algorithm>
#include <cinttypes>
#include <fcntl.h>
#include <fstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dfx_log.h"
#include "minidump_optimizer.h"
#include "minidump_config.h"
#include "minidump_format.h"
#include "minidump_memory_reader.h"
#include "minidump_optimizer.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
using namespace MinidumpFormat;
constexpr size_t DEFAULT_READ_AHEAD_SIZE = 65536;
MinidumpMemoryReader::MinidumpMemoryReader(std::shared_ptr<std::istream> stream)
    : backend_(BackendType::ISTREAM), stream_(stream)
{
}

MinidumpMemoryReader::MinidumpMemoryReader(const std::string& path)
    : backend_(BackendType::MMAP)
{
    if (!InitMmap(path)) {
        backend_ = BackendType::ISTREAM;
        auto fileStream = std::make_shared<std::ifstream>();
        streamBuf_ = new char[DEFAULT_READ_AHEAD_SIZE];
        fileStream->rdbuf()->pubsetbuf(streamBuf_, DEFAULT_READ_AHEAD_SIZE);
        fileStream->open(path, std::ios::binary);
        stream_ = fileStream;
    }
}

MinidumpMemoryReader::~MinidumpMemoryReader()
{
    if (mmapData_ != nullptr) {
        munmap(mmapData_, mmapSize_);
        mmapData_ = nullptr;
    }
    if (mmapFd_ >= 0) {
        close(mmapFd_);
        mmapFd_ = -1;
    }
    if (streamBuf_ != nullptr) {
        stream_.reset();
        delete[] streamBuf_;
    }
}

bool MinidumpMemoryReader::InitMmap(const std::string& path)
{
    mmapFd_ = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (mmapFd_ < 0) {
        DFXLOGE("MinidumpMemoryReader open failed for %{public}s errno=%{public}d", path.c_str(), errno);
        return false;
    }

    struct stat st;
    if (fstat(mmapFd_, &st) != 0) {
        DFXLOGE("MinidumpMemoryReader fstat failed errno=%{public}d", errno);
        close(mmapFd_);
        mmapFd_ = -1;
        return false;
    }

    fileSize_ = static_cast<size_t>(st.st_size);
    if (fileSize_ == 0) {
        DFXLOGE("MinidumpMemoryReader file is empty");
        close(mmapFd_);
        mmapFd_ = -1;
        return false;
    }

    void* mapped = mmap(nullptr, fileSize_, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mmapFd_, 0);
    if (mapped == MAP_FAILED) {
        DFXLOGW("MinidumpMemoryReader mmap failed errno=%{public}d, fallback to istream", errno);
        close(mmapFd_);
        mmapFd_ = -1;
        return false;
    }

    mmapData_ = static_cast<uint8_t*>(mapped);
    mmapSize_ = fileSize_;
    mmapPos_ = 0;
    madvise(mmapData_, mmapSize_, MADV_RANDOM);
    DFXLOGI("MinidumpMemoryReader mmap enabled, size=%{public}zu", fileSize_);
    return true;
}

bool MinidumpMemoryReader::ReadFromMmap(void* bytes, size_t count)
{
    if (mmapPos_ < 0 || static_cast<size_t>(mmapPos_) + count > mmapSize_) {
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementError();
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_READ, "mmap read out of range", __LINE__);
        return false;
    }
    if (count > 0) {
        if (memcpy_s(bytes, count, mmapData_ + mmapPos_, count) != EOK) {
            auto& stats = MinidumpPerfMonitor::Instance().GetStats();
            stats.IncrementError();
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_READ, "memcpy_s failed", __LINE__);
            return false;
        }
        mmapPos_ += static_cast<off_t>(count);
    }
    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    stats.IncrementRead(static_cast<uint32_t>(count));
    return true;
}

bool MinidumpMemoryReader::ReadFromStream(void* bytes, size_t count)
{
    if (stream_ == nullptr || (!stream_->good() && !stream_->eof())) {
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementError();
        return false;
    }
    stream_->clear(stream_->rdstate() & ~std::ios::eofbit);
    stream_->read(static_cast<char*>(bytes), count);
    if (static_cast<size_t>(stream_->gcount()) != count) {
        stream_->setstate(std::ios::badbit);
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementError();
        return false;
    }
    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    stats.IncrementRead(static_cast<uint32_t>(count));
    return true;
}

bool MinidumpMemoryReader::ReadBytes(void* bytes, size_t count)
{
    if (bytes == nullptr && count > 0) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    if (backend_ == BackendType::MMAP && mmapData_ != nullptr) {
        return ReadFromMmap(bytes, count);
    }
    return ReadFromStream(bytes, count);
}

bool MinidumpMemoryReader::SeekSet(off_t offset)
{
    if (offset < 0) {
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementError();
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "negative offset", __LINE__);
        return false;
    }

    if (backend_ == BackendType::MMAP && mmapData_ != nullptr) {
        if (static_cast<size_t>(offset) > mmapSize_) {
            auto& stats = MinidumpPerfMonitor::Instance().GetStats();
            stats.IncrementError();
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "mmap seek out of range", __LINE__);
            return false;
        }
        mmapPos_ = offset;
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementSeek();
        return true;
    }

    if (!stream_) {
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementError();
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "stream is null", __LINE__);
        return false;
    }

    stream_->clear(stream_->rdstate() & ~std::ios::eofbit);
    stream_->seekg(offset, std::ios::beg);
    if (stream_->fail() || stream_->tellg() != static_cast<std::streampos>(offset)) {
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.IncrementError();
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "seek failed", __LINE__);
        return false;
    }
    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    stats.IncrementSeek();
    return true;
}

off_t MinidumpMemoryReader::Tell()
{
    if (backend_ == BackendType::MMAP && mmapData_ != nullptr) {
        return mmapPos_;
    }
    if (!stream_ || (!stream_->good() && !stream_->eof())) {
        return -1;
    }
    return stream_->tellg();
}

std::string MinidumpMemoryReader::ConvertUTF16ToUTF8(const std::vector<uint16_t>& in)
{
    if (in.empty()) {
        return "";
    }
    std::string out;
    out.reserve(in.size() * 3); // 3 : max UTF-8 bytes per UTF-16 code unit

    for (size_t i = 0; i < in.size() && in[i]; ++i) {
        uint32_t cp = in[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if ((i + 1) < in.size() && in[i + 1] >= 0xDC00 && in[i + 1] <= 0xDFFF) {
                uint16_t low = in[i + 1];
                cp = (cp << 10) + low - 0x35FDC00; // 10 : Shift left by 10 bit
                ++i;
            } else {
                return "";
            }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return "";
        }

        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6))); // 6 : Shift right by 6 bit
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12))); // 12 : Shift right by 12 bit
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); // 6 : Shift right by 6 bit
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x110000) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18))); // 18 : Shift right by 18 bit
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); // 12 : Shift right by 12 bit
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); // 6 : Shift right by 6 bit
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            return "";
        }
    }
    return out;
}

std::shared_ptr<std::string> MinidumpMemoryReader::ReadString(off_t offset)
{
    if (!SeekSet(offset)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "Cannot seek to string offset", __LINE__);
        DFXLOGE("MinidumpMemoryReader could not seek to string at offset %{public}" PRIX64, offset);
        return nullptr;
    }

    MDString mdString;
    if (!ReadBytes(&(mdString.length), sizeof(mdString.length))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STRING_READ, "Cannot read string length", __LINE__);
        return nullptr;
    }

    if (mdString.length == 0 || mdString.length % 2 != 0) { // 2 : length must be even number
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STRING_LENGTH, "Invalid string length", __LINE__);
        return nullptr;
    }
    uint32_t utf16Length = mdString.length / 2;
    if (utf16Length == 0 || utf16Length > MinidumpConfigManager::Instance().GetConfig().maxStringLength) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STRING_LENGTH, "String length exceeds maximum", __LINE__);
        return nullptr;
    }

    std::vector<uint16_t> utf16Data(utf16Length);
    if (!ReadBytes(utf16Data.data(), mdString.length)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STRING_READ, "Cannot read UTF16 string data", __LINE__);
        return nullptr;
    }

    auto out = std::make_shared<std::string>(ConvertUTF16ToUTF8(utf16Data));
    lastError_.Clear();
    return out;
}

bool MinidumpMemoryReader::ReadUTF8String(off_t offset, std::string* utf8Str)
{
    if (utf8Str == nullptr) {
        return false;
    }
    auto str = ReadString(offset);
    if (str == nullptr) {
        return false;
    }
    *utf8Str = *str;
    return true;
}
}
}