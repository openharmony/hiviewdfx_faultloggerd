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

#include "minidump_observer.h"

#include <algorithm>
#include <chrono>

namespace OHOS {
namespace HiviewDFX {

MinidumpEvent::MinidumpEvent(MinidumpEventType t)
    : type(t), progress(0), total(0),
      message(""), address(0), size(0), timestamp(0), streamName("")
{}

bool IMinidumpObserver::ShouldHandle(MinidumpEventType type) const
{
    return true;
}

ProgressObserver::ProgressObserver(
    std::function<void(uint32_t, uint32_t, const std::string&)> callback)
    : callback_(callback)
{}

void ProgressObserver::OnEvent(const MinidumpEvent& event)
{
    if (event.type == MinidumpEventType::PARSE_PROGRESS ||
        event.type == MinidumpEventType::PARSE_START ||
        event.type == MinidumpEventType::PARSE_COMPLETE) {
        if (callback_) {
            callback_(event.progress, event.total, event.message);
        }
    }
}

bool ProgressObserver::ShouldHandle(MinidumpEventType type) const
{
    return type == MinidumpEventType::PARSE_PROGRESS ||
           type == MinidumpEventType::PARSE_START ||
           type == MinidumpEventType::PARSE_COMPLETE;
}

StreamLoadObserver::StreamLoadObserver(
    std::function<void(const std::string&, uint32_t)> callback)
    : callback_(callback) {}

void StreamLoadObserver::OnEvent(const MinidumpEvent& event)
{
    if (event.type == MinidumpEventType::STREAM_LOADED) {
        if (callback_) {
            callback_(event.streamName, event.progress);
        }
    }
}

bool StreamLoadObserver::ShouldHandle(MinidumpEventType type) const
{
    return type == MinidumpEventType::STREAM_LOADED;
}

MinidumpObservable::MinidumpObservable() : observers_(), mutex_() {}

void MinidumpObservable::AddObserver(std::shared_ptr<IMinidumpObserver> observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it == observers_.end()) {
        observers_.push_back(observer);
    }
}

void MinidumpObservable::RemoveObserver(std::shared_ptr<IMinidumpObserver> observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end()) {
        observers_.erase(it);
    }
}

void MinidumpObservable::NotifyObservers(const MinidumpEvent& event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& observer : observers_) {
        if (observer && observer->ShouldHandle(event.type)) {
            observer->OnEvent(event);
        }
    }
}

void MinidumpObservable::ClearObservers()
{
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.clear();
}

size_t MinidumpObservable::GetObserverCount()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return observers_.size();
}

MinidumpSubject::MinidumpSubject() : MinidumpObservable(), currentProgress_(0), totalProgress_(0) {}

void MinidumpSubject::NotifyParseStart(const std::string& path)
{
    MinidumpEvent event(MinidumpEventType::PARSE_START);
    event.message = path;
    event.timestamp = GetCurrentTimestamp();
    NotifyObservers(event);
}

void MinidumpSubject::NotifyParseProgress(uint32_t current, uint32_t total, const std::string& message)
{
    currentProgress_ = current;
    totalProgress_ = total;

    MinidumpEvent event(MinidumpEventType::PARSE_PROGRESS);
    event.progress = current;
    event.total = total;
    event.message = message;
    event.timestamp = GetCurrentTimestamp();
    NotifyObservers(event);
}

void MinidumpSubject::NotifyParseComplete(bool success)
{
    MinidumpEvent event(MinidumpEventType::PARSE_COMPLETE);
    event.progress = currentProgress_;
    event.total = totalProgress_;
    event.message = success ? "Parse complete succes" : "Parse complete failed";
    event.timestamp = GetCurrentTimestamp();
    NotifyObservers(event);
}

void MinidumpSubject::NotifyStreamLoaded(const std::string& streamName, uint32_t count)
{
    MinidumpEvent event(MinidumpEventType::STREAM_LOADED);
    event.streamName = streamName;
    event.progress = count;
    event.timestamp = GetCurrentTimestamp();
    NotifyObservers(event);
}

uint64_t MinidumpSubject::GetCurrentTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

}
}