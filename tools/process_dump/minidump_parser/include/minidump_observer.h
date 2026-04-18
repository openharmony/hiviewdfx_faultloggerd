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

#ifndef MINIDUMP_OBSERVER_H
#define MINIDUMP_OBSERVER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "minidump_error.h"

namespace OHOS {
namespace HiviewDFX {

enum class MinidumpEventType : uint32_t {
    PARSE_START           = 0,
    PARSE_PROGRESS        = 1,
    PARSE_COMPLETE        = 2,
    STREAM_LOADED         = 3,
    CONFIG_CHANGED        = 4,
};

struct MinidumpEvent {
    MinidumpEventType type;
    uint32_t progress;
    uint32_t total;
    MinidumpError error;
    std::string message;
    uint64_t address;
    uint32_t size;
    uint64_t timestamp;
    std::string streamName;

    explicit MinidumpEvent(MinidumpEventType t);
};

class IMinidumpObserver {
public:
    virtual ~IMinidumpObserver() = default;
    virtual void OnEvent(const MinidumpEvent& event) = 0;
    virtual bool ShouldHandle(MinidumpEventType type) const;
};

class ProgressObserver : public IMinidumpObserver {
public:
    explicit ProgressObserver(
        std::function<void(uint32_t, uint32_t, const std::string&)> callback);

    void OnEvent(const MinidumpEvent& event) override;
    bool ShouldHandle(MinidumpEventType type) const override;

private:
    std::function<void(uint32_t, uint32_t, const std::string&)> callback_;
};

class StreamLoadObserver : public IMinidumpObserver {
public:
    explicit StreamLoadObserver(
        std::function<void(const std::string&, uint32_t)> callback);

    void OnEvent(const MinidumpEvent& event) override;
    bool ShouldHandle(MinidumpEventType type) const override;

private:
    std::function<void(const std::string&, uint32_t)> callback_;
};

class MinidumpObservable {
public:
    MinidumpObservable();

    void AddObserver(std::shared_ptr<IMinidumpObserver> observer);
    void RemoveObserver(std::shared_ptr<IMinidumpObserver> observer);
    void NotifyObservers(const MinidumpEvent& event);
    void ClearObservers();
    size_t GetObserverCount();

protected:
    std::vector<std::shared_ptr<IMinidumpObserver>> observers_;
    std::mutex mutex_;
};

class MinidumpSubject : public MinidumpObservable {
public:
    MinidumpSubject();

    void NotifyParseStart(const std::string& path);
    void NotifyParseProgress(uint32_t current, uint32_t total, const std::string& message);
    void NotifyParseComplete(bool success);
    void NotifyStreamLoaded(const std::string& streamName, uint32_t count);

private:
    uint64_t GetCurrentTimestamp();

    uint32_t currentProgress_;
    uint32_t totalProgress_;
};
}
}
#endif