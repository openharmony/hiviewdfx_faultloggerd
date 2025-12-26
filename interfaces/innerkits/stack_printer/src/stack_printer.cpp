/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "stack_printer.h"

#include <iostream>
#include <mutex>
#include <sstream>

#include <sys/prctl.h>

#include "dfx_frame.h"
#include "dfx_frame_formatter.h"
#include "unique_stack_table.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr uint64_t SEC_TO_NANOSEC = 1000000000;
constexpr uint64_t MICROSEC_TO_NANOSEC = 1000;
constexpr int FORMAT_TIME_LEN = 20;
constexpr int MICROSEC_LEN = 6;
constexpr size_t MAX_SAMPLE_TIDS = 10;
constexpr int MAX_TID_VALUE = 65536;
}  // namespace

struct StackRecord {
    uint64_t stackId {0};
    std::vector<uint64_t> snapshotTimes;
};

struct StackItem {
    uintptr_t pc {0};
    int32_t pcCount {0};
    uint32_t level {0};
    uint64_t stackId {0};  // Only leaf node update this.
    std::shared_ptr<DfxFrame> current {nullptr};
    std::shared_ptr<StackItem> child {nullptr};
    std::shared_ptr<StackItem> siblings {nullptr};
};

std::ostream& operator<<(std::ostream& os, const SampledFrame& frame)
{
    os << frame.indent << " " << frame.count << " " << frame.level << " " << frame.pc << " " << frame.isLeaf << " " <<
          frame.timestamps.size();
    for (auto timestamp : frame.timestamps) {
        os << " " << timestamp;
    }
    return os;
}

std::istream& operator>>(std::istream& is, SampledFrame& frame)
{
    size_t timestampsSize;
    is >> frame.indent >> frame.count >> frame.level >> frame.pc >> frame.isLeaf >> timestampsSize;
    if (timestampsSize > 0) {
        frame.timestamps.resize(timestampsSize);
        for (size_t i = 0; i < timestampsSize; i++) {
            is >> frame.timestamps[i];
        }
    }
    return is;
}

static std::string TimeFormat(uint64_t time)
{
    uint64_t nsec = time % SEC_TO_NANOSEC;
    time_t sec = static_cast<time_t>(time / SEC_TO_NANOSEC);
    char timeChars[FORMAT_TIME_LEN];
    struct tm localTime;
    if (localtime_r(&sec, &localTime) == nullptr) {
        return "";
    }
    size_t sz = strftime(timeChars, FORMAT_TIME_LEN, "%Y-%m-%d-%H-%M-%S", &localTime);
    if (sz == 0) {
        return "";
    }
    std::string s = timeChars;
    uint64_t usec = nsec / MICROSEC_TO_NANOSEC;
    std::string usecStr = std::to_string(usec);
    while (usecStr.size() < MICROSEC_LEN) {
        usecStr = "0" + usecStr;
    }
    s = s + "." + usecStr;
    return s;
}

static std::vector<StackRecord> TimeFilter(const std::vector<StackRecord>& stackRecordVec, uint64_t beginTime,
                                           uint64_t endTime)
{
    if ((beginTime == 0 && endTime == 0) || (beginTime > endTime)) {
        return stackRecordVec;
    }
    std::vector<StackRecord> vec;
    for (const auto& stackRecord : stackRecordVec) {
        StackRecord record;
        for (auto t : stackRecord.snapshotTimes) {
            if (t >= beginTime && t < endTime) {
                record.snapshotTimes.emplace_back(t);
            }
        }
        if (!record.snapshotTimes.empty()) {
            record.stackId = stackRecord.stackId;
            vec.emplace_back(std::move(record));
        }
    }
    return vec;
}

class StackPrinter::Impl {
public:
    Impl() : root_(nullptr)
    {}

    inline void SetUnwindInfo(const std::shared_ptr<Unwinder>& unwinder, const std::shared_ptr<DfxMaps>& maps)
    {
        unwinder_ = unwinder;
        maps_ = maps;
    }

    bool InitUniqueTable(pid_t pid, uint32_t size, std::string name = "unique_stack_table");
    bool PutPcsInTable(const std::vector<uintptr_t>& pcs, int tid, uint64_t snapshotTime);
    std::string GetFullStack(const std::vector<TimeStampedPcs>& timeStampedPcsVec);
    std::map<int, std::vector<SampledFrame>> GetThreadSampledFrames(uint64_t beginTime = 0, uint64_t endTime = 0);
    std::string GetTreeStack(int tid, bool printTimes = false, uint64_t beginTime = 0, uint64_t endTime = 0);
    std::string GetHeaviestStack(int tid, uint64_t beginTime = 0, uint64_t endTime = 0);

    static std::string PrintTreeStackBySampledStack(const std::vector<SampledFrame>& sampledFrameVec, bool printTimes,
                                                    const std::shared_ptr<Unwinder>& unwinder,
                                                    const std::shared_ptr<DfxMaps>& maps);
    static void SerializeSampledFrameMap(const std::map<int, std::vector<SampledFrame>>& sampledFrameMap,
                                         std::ostream& os);
    static std::map<int, std::vector<SampledFrame>> DeserializeSampledFrameMap(std::istream& is);

private:
    void Insert(const std::vector<uintptr_t>& pcs, int32_t count, StackId stackId);
    std::shared_ptr<StackItem> InsertImpl(std::shared_ptr<StackItem> curNode, uintptr_t pc, int32_t count,
                                          uint64_t level, std::shared_ptr<StackItem> acientNode);
    std::shared_ptr<StackItem> AdjustSiblings(std::shared_ptr<StackItem> acient, std::shared_ptr<StackItem> cur,
                                              std::shared_ptr<StackItem> node);
    void BuildStackTree(std::vector<StackRecord>& stackRecordVec);
    std::vector<SampledFrame> GenerateTreeStackFrames(const std::vector<StackRecord>& stackRecordVec);
    std::vector<SampledFrame> GetSampledFramesByTid(int tid, uint64_t beginTime, uint64_t endTime);

    static void PrintTimesStr(std::string& frameStr, const std::vector<uint64_t>& snapshotTimes);
    static std::string PrintStackByPcs(const std::vector<uintptr_t>& pcs, const std::shared_ptr<Unwinder>& unwinder,
                                       const std::shared_ptr<DfxMaps>& maps);

    std::shared_ptr<StackItem> root_;
    std::shared_ptr<Unwinder> unwinder_ {nullptr};
    std::shared_ptr<DfxMaps> maps_ {nullptr};
    std::unique_ptr<UniqueStackTable> uniqueStackTable_ {nullptr};
    std::map<int, std::vector<StackRecord>> tidStackRecordMap_;
    std::mutex mutex_;
};

StackPrinter::StackPrinter() : impl_(std::make_shared<Impl>())
{}

void StackPrinter::SetUnwindInfo(const std::shared_ptr<Unwinder>& unwinder, const std::shared_ptr<DfxMaps>& maps)
{
    impl_->SetUnwindInfo(unwinder, maps);
}

bool StackPrinter::PutPcsInTable(const std::vector<uintptr_t>& pcs, int tid, uint64_t snapshotTime)
{
    return impl_->PutPcsInTable(pcs, tid, snapshotTime);
}

std::string StackPrinter::GetFullStack(const std::vector<TimeStampedPcs>& timeStampedPcsVec)
{
    return impl_->GetFullStack(timeStampedPcsVec);
}

std::map<int, std::vector<SampledFrame>> StackPrinter::GetThreadSampledFrames(uint64_t beginTime, uint64_t endTime)
{
    return impl_->GetThreadSampledFrames(beginTime, endTime);
}

std::string StackPrinter::GetHeaviestStack(int tid, uint64_t beginTime, uint64_t endTime)
{
    return impl_->GetHeaviestStack(tid, beginTime, endTime);
}

bool StackPrinter::InitUniqueTable(pid_t pid, uint32_t size, std::string name)
{
    return impl_->InitUniqueTable(pid, size, name);
}

std::string StackPrinter::PrintTreeStackBySampledStack(const std::vector<SampledFrame>& sampledFrameVec,
                                                       bool printTimes, const std::shared_ptr<Unwinder>& unwinder,
                                                       const std::shared_ptr<DfxMaps>& maps)
{
    return Impl::PrintTreeStackBySampledStack(sampledFrameVec, printTimes, unwinder, maps);
}

std::string StackPrinter::GetTreeStack(int tid, bool printTimes, uint64_t beginTime, uint64_t endTime)
{
    return impl_->GetTreeStack(tid, printTimes, beginTime, endTime);
}

void StackPrinter::SerializeSampledFrameMap(const std::map<int, std::vector<SampledFrame>>& sampledFrameMap,
                                            std::ostream& os)
{
    Impl::SerializeSampledFrameMap(sampledFrameMap, os);
}

std::map<int, std::vector<SampledFrame>> StackPrinter::DeserializeSampledFrameMap(std::istream& is)
{
    return Impl::DeserializeSampledFrameMap(is);
}

std::shared_ptr<StackItem> StackPrinter::Impl::InsertImpl(std::shared_ptr<StackItem> curNode, uintptr_t pc,
                                                          int32_t pcCount, uint64_t level,
                                                          std::shared_ptr<StackItem> acientNode)
{
    if (curNode == nullptr) {
        return nullptr;
    }

    if (level ==  curNode-> level && curNode->pc == pc) {
        curNode->pcCount += pcCount;
        return curNode;
    }

    if (curNode->pc == 0) {
        curNode->pc = pc;
        curNode->pcCount += pcCount;
        return curNode;
    }

    if (level > curNode->level) {
        acientNode = curNode;

        if (curNode->child == nullptr) {
            curNode->child = std::make_shared<StackItem>();
            curNode->child->level = curNode->level + 1;
            return InsertImpl(curNode->child, pc, pcCount, level, acientNode);
        }

        if (curNode->child->pc == pc) {
            curNode->child->pcCount += pcCount;
            return curNode->child;
        }

        curNode = curNode->child;
    }

    if (curNode->siblings == nullptr) {
        curNode->siblings = std::make_shared<StackItem>();
        curNode->siblings->level = curNode->level;
        std::shared_ptr<StackItem> node = InsertImpl(curNode->siblings, pc, pcCount, level, acientNode);
        curNode = AdjustSiblings(acientNode, curNode, node);
        return curNode;
    }

    if (curNode->siblings->pc == pc) {
        curNode->siblings->pcCount += pcCount;
        curNode = AdjustSiblings(acientNode, curNode, curNode->siblings);
        return curNode;
    }

    return InsertImpl(curNode->siblings, pc, pcCount, level, acientNode);
}

void StackPrinter::Impl::Insert(const std::vector<uintptr_t>& pcs, int32_t pcCount, StackId stackId)
{
    if (root_ == nullptr) {
        root_ = std::make_shared<StackItem>();
        root_->level = 0;
    }

    std::shared_ptr<StackItem> dummyNode = std::make_shared<StackItem>();
    std::shared_ptr<StackItem> acientNode = dummyNode;
    acientNode->child = root_;
    std::shared_ptr<StackItem> curNode = root_;
    uint64_t level = 0;
    for (auto iter = pcs.rbegin(); iter != pcs.rend(); ++iter) {
        curNode = InsertImpl(curNode, *iter, pcCount, level, acientNode);
        level++;
    }
    curNode->stackId = stackId.value;
}

std::shared_ptr<StackItem> StackPrinter::Impl::AdjustSiblings(std::shared_ptr<StackItem> acient,
                                                              std::shared_ptr<StackItem> cur,
                                                              std::shared_ptr<StackItem> node)
{
    std::shared_ptr<StackItem> dummy = std::make_shared<StackItem>();
    dummy->siblings = acient->child;
    std::shared_ptr<StackItem> p = dummy;
    while (p->siblings != node && p->siblings->pcCount >= node->pcCount) {
        p = p->siblings;
    }
    if (p->siblings != node) {
        cur->siblings = node->siblings;
        node->siblings = p->siblings;
        if (p == dummy) {
            acient->child = node;
        }
        p->siblings = node;
    }
    return node;
}

bool StackPrinter::Impl::PutPcsInTable(const std::vector<uintptr_t>& pcs, int tid, uint64_t snapshotTime)
{
    if (uniqueStackTable_ == nullptr) {
        return false;
    }
    uint64_t stackId = 0;
    auto stackIdPtr = reinterpret_cast<StackId*>(&stackId);
    uniqueStackTable_->PutPcsInTable(stackIdPtr, pcs.data(), pcs.size());

    std::lock_guard<std::mutex> lock(mutex_);
    auto& stackRecordVec = tidStackRecordMap_[tid];
    auto it = std::find_if(stackRecordVec.begin(), stackRecordVec.end(), [&stackId](const auto& stackRecord) {
        return stackRecord.stackId == stackId;
    });
    if (it == stackRecordVec.end()) {
        StackRecord record;
        record.stackId = stackId;
        record.snapshotTimes.emplace_back(snapshotTime);
        stackRecordVec.emplace_back(record);
    } else {
        it->snapshotTimes.emplace_back(snapshotTime);
    }
    return true;
}

std::string StackPrinter::Impl::GetFullStack(const std::vector<TimeStampedPcs>& timeStampedPcsVec)
{
    std::stringstream stack;
    for (const auto& timeStampedPcs : timeStampedPcsVec) {
        std::string snapshotTimeStr = TimeFormat(timeStampedPcs.snapshotTime);
        stack << "SnapshotTime:" << snapshotTimeStr << "\n";
        std::vector<uintptr_t> pcs = timeStampedPcs.pcVec;
        stack << PrintStackByPcs(pcs, unwinder_, maps_);
        stack << "\n";
    }
    return stack.str();
}

std::vector<SampledFrame> StackPrinter::Impl::GetSampledFramesByTid(int tid, uint64_t beginTime, uint64_t endTime)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& stackRecordVec = tidStackRecordMap_[tid];
    std::vector<StackRecord> vec = TimeFilter(stackRecordVec, beginTime, endTime);
    BuildStackTree(vec);
    return GenerateTreeStackFrames(vec);
}

std::string StackPrinter::Impl::GetTreeStack(int tid, bool printTimes, uint64_t beginTime, uint64_t endTime)
{
    std::vector<SampledFrame> sampledFrameVec = GetSampledFramesByTid(tid, beginTime, endTime);
    return PrintTreeStackBySampledStack(sampledFrameVec, printTimes, unwinder_, maps_);
}

std::string StackPrinter::Impl::GetHeaviestStack(int tid, uint64_t beginTime, uint64_t endTime)
{
    if (unwinder_ == nullptr || maps_ == nullptr) {
        return std::string("");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto& stackRecordVec = tidStackRecordMap_[tid];
    std::vector<StackRecord> vec = TimeFilter(stackRecordVec, beginTime, endTime);
    if (vec.empty()) {
        return std::string("");
    }
    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
        return a.snapshotTimes.size() > b.snapshotTimes.size();
    });
    auto it = vec.begin();
    std::vector<uintptr_t> pcs;
    StackId stackId;
    stackId.value = it->stackId;
    uniqueStackTable_->GetPcsByStackId(stackId, pcs);

    std::stringstream heaviestStack;
    heaviestStack << "heaviest stack: \nstack counts: " << std::to_string(it->snapshotTimes.size()) << "\n";
    heaviestStack << PrintStackByPcs(pcs, unwinder_, maps_);
    return heaviestStack.str();
}

bool StackPrinter::Impl::InitUniqueTable(pid_t pid, uint32_t size, std::string name)
{
    uniqueStackTable_ = std::make_unique<UniqueStackTable>(pid, size);
    if (!uniqueStackTable_->Init()) {
        return false;
    }
    void* uniqueTableBufMMap = reinterpret_cast<void*>(uniqueStackTable_->GetHeadNode());
    if (!name.empty()) {
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, uniqueTableBufMMap, size, name.c_str());
    }
    return true;
}

void StackPrinter::Impl::BuildStackTree(std::vector<StackRecord>& stackRecordVec)
{
    std::sort(stackRecordVec.begin(), stackRecordVec.end(), [](const auto& a, const auto& b) {
        return a.snapshotTimes.size() > b.snapshotTimes.size();
    });
    for (const auto& record : stackRecordVec) {
        std::vector<uintptr_t> pcs;
        StackId stackId;
        stackId.value = record.stackId;
        if (uniqueStackTable_->GetPcsByStackId(stackId, pcs)) {
            Insert(pcs, record.snapshotTimes.size(), stackId);
        }
    }
}

void StackPrinter::Impl::PrintTimesStr(std::string& frameStr, const std::vector<uint64_t>& snapshotTimes)
{
    // rm last '\n'
    auto pos = frameStr.find("\n");
    if (pos != std::string::npos) {
        frameStr.replace(pos, 1, "");
    }

    for (auto t : snapshotTimes) {
        frameStr.append(" ");
        frameStr.append(TimeFormat(t));
    }
    frameStr.append("\n");
}

std::string StackPrinter::Impl::PrintTreeStackBySampledStack(const std::vector<SampledFrame>& sampledFrameVec,
                                                             bool printTimes, const std::shared_ptr<Unwinder>& unwinder,
                                                             const std::shared_ptr<DfxMaps>& maps)
{
    if (unwinder == nullptr || maps == nullptr) {
        return std::string("");
    }

    std::stringstream ss;
    for (const auto& sampledFrame : sampledFrameVec) {
        std::string space(sampledFrame.indent, ' ');
        DfxFrame frame;
        frame.index = sampledFrame.level;
        if (!unwinder->GetFrameByPc(sampledFrame.pc, maps, frame)) {
            frame.pc = 0;
        }
        std::string frameStr = DfxFrameFormatter::GetFrameStr(frame);

        if (sampledFrame.isLeaf && printTimes) {
            PrintTimesStr(frameStr, sampledFrame.timestamps);
        }
        ss << space << sampledFrame.count << " " << frameStr;
    }
    return ss.str();
}

std::vector<SampledFrame> StackPrinter::Impl::GenerateTreeStackFrames(const std::vector<StackRecord>& stackRecordVec)
{
    std::vector<SampledFrame> sampledFrameVec;
    std::vector<std::shared_ptr<StackItem>> nodes;
    if (root_ != nullptr) {
        nodes.emplace_back(root_);
    }
    const uint32_t indent = 2;
    while (!nodes.empty()) {
        std::shared_ptr<StackItem> back = nodes.back();
        SampledFrame sampledFrame;
        sampledFrame.indent = indent * back->level;
        sampledFrame.count = back->pcCount;
        sampledFrame.level = back->level;
        sampledFrame.pc = back->pc;
        nodes.pop_back();

        if (back->siblings != nullptr) {
            nodes.emplace_back(back->siblings);
        }

        if (back->child != nullptr) {
            nodes.emplace_back(back->child);
            sampledFrame.isLeaf = false;
        } else {
            sampledFrame.isLeaf = true;
            uint64_t stackId = back->stackId;
            auto it = std::find_if(stackRecordVec.begin(), stackRecordVec.end(), [&stackId](const auto& record) {
                return record.stackId == stackId;
            });
            if (it != stackRecordVec.end()) {
                sampledFrame.timestamps = it->snapshotTimes;
            }
        }
        sampledFrameVec.emplace_back(sampledFrame);
    }
    root_ = nullptr;
    return sampledFrameVec;
}

std::string StackPrinter::Impl::PrintStackByPcs(const std::vector<uintptr_t>& pcs,
                                                const std::shared_ptr<Unwinder>& unwinder,
                                                const std::shared_ptr<DfxMaps>& maps)
{
    std::stringstream ss;
    for (size_t i = 0; i < pcs.size(); i++) {
        DfxFrame frame;
        unwinder->GetFrameByPc(pcs[i], maps, frame);
        frame.index = i;
        auto frameStr = DfxFrameFormatter::GetFrameStr(frame);
        ss << frameStr;
    }
    return ss.str();
}

std::map<int, std::vector<SampledFrame>> StackPrinter::Impl::GetThreadSampledFrames(uint64_t beginTime,
                                                                                    uint64_t endTime)
{
    std::map<int, std::vector<SampledFrame>> sampledFrameMap;
    for (const auto& [tid, _] : tidStackRecordMap_) {
        sampledFrameMap[tid] = GetSampledFramesByTid(tid, beginTime, endTime);
    }
    return sampledFrameMap;
}

void StackPrinter::Impl::SerializeSampledFrameMap(const std::map<int, std::vector<SampledFrame>>& sampledFrameMap,
                                                  std::ostream& os)
{
    os << sampledFrameMap.size() << "\n";
    for (const auto& [tid, sampledFrameVec] : sampledFrameMap) {
        os << tid << " " << sampledFrameVec.size();
        for (const auto& frame : sampledFrameVec) {
            os << " " << frame;
        }
        os << "\n";
    }
}

std::map<int, std::vector<SampledFrame>> StackPrinter::Impl::DeserializeSampledFrameMap(std::istream& is)
{
    std::map<int, std::vector<SampledFrame>> sampledFrameMap;
    size_t mapSize;
    if (!(is >> mapSize)) {
        is.setstate(std::ios::failbit);
        return std::map<int, std::vector<SampledFrame>>();
    }
    if (mapSize > MAX_SAMPLE_TIDS) {
        return std::map<int, std::vector<SampledFrame>>();
    }
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    for (size_t i = 0; i < mapSize; i++) {
        int tid;
        size_t vecSize;
        if (!(is >> tid >> vecSize)) {
            is.setstate(std::ios::failbit);
            return std::map<int, std::vector<SampledFrame>>();
        }
        std::vector<SampledFrame> sampledFrameVec;
        for (size_t j = 0; j < vecSize; j++) {
            SampledFrame frame;
            if (!(is >> frame)) {
                is.setstate(std::ios::failbit);
                return std::map<int, std::vector<SampledFrame>>();
            }
            sampledFrameVec.emplace_back(frame);
        }
        is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (tid < 0 || tid > MAX_TID_VALUE) {
            return std::map<int, std::vector<SampledFrame>>();
        }
        sampledFrameMap[tid] = sampledFrameVec;
    }
    return sampledFrameMap;
}
}  // end of namespace HiviewDFX
}  // end of namespace OHOS
