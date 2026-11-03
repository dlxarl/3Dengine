#include "Log.h"

#include <iostream>
#include <mutex>
#include <utility>
#include <vector>

namespace {
    struct SinkEntry {
        int handle;
        Log::Sink sink;
    };

    struct LogState {
        std::mutex mutex;
        std::vector<SinkEntry> sinks;
        int nextHandle = 1;
    };

    LogState& state() {
        static LogState instance;
        return instance;
    }

    const char* levelTag(LogLevel level) {
        switch (level) {
            case LogLevel::Warning: return "warning";
            case LogLevel::Error:   return "error";
            default:                return "info";
        }
    }
}

int Log::addSink(Sink sink) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    int handle = s.nextHandle++;
    s.sinks.push_back({handle, std::move(sink)});
    return handle;
}

void Log::removeSink(int handle) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    for (auto it = s.sinks.begin(); it != s.sinks.end(); ++it) {
        if (it->handle == handle) {
            s.sinks.erase(it);
            return;
        }
    }
}

void Log::write(LogLevel level, const char* category, const std::string& message) {
    std::ostream& stream = (level == LogLevel::Info) ? std::cout : std::cerr;
    stream << "[" << levelTag(level) << "][" << category << "] " << message << "\n";

    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    for (const auto& entry : s.sinks) {
        entry.sink(level, category, message);
    }
}
