#pragma once
#include "Log.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

struct ConsoleEntry {
    LogLevel level;
    std::string category;
    std::string message;
    double time;  // seconds since the Editor window opened
};

// Keeps recent log messages for the Console panel. It registers itself with the engine's log,
// which may report from any thread, so access goes through a lock.
class ConsoleLog {
public:
    ConsoleLog();
    ~ConsoleLog();

    ConsoleLog(const ConsoleLog&) = delete;
    ConsoleLog& operator=(const ConsoleLog&) = delete;

    void clear();

    // Calls visit(entry) under the lock, oldest first.
    template <typename Visitor> void forEach(Visitor&& visit) const {
        std::lock_guard<std::mutex> lock(mutex);
        for (const ConsoleEntry& entry : entries) {
            visit(entry);
        }
    }

    int count(LogLevel level) const;

    // Bumps whenever the contents change, so that the panel only re-filters when needed.
    uint64_t getRevision() const;

    bool getLast(ConsoleEntry& entry) const;

private:
    mutable std::mutex mutex;
    std::deque<ConsoleEntry> entries;
    int counts[3] = {0, 0, 0};
    uint64_t revision = 0;
    int sink = 0;
};
