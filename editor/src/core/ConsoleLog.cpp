#include "ConsoleLog.h"

#include <GLFW/glfw3.h>

namespace {
    const size_t MAX_ENTRIES = 5000;
}

ConsoleLog::ConsoleLog() {
    sink = Log::addSink([this](LogLevel level, const char* category, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);

        if (entries.size() >= MAX_ENTRIES) {
            counts[(int)entries.front().level]--;
            entries.pop_front();
        }

        entries.push_back({level, category, message, glfwGetTime()});
        counts[(int)level]++;
        revision++;
    });
}

ConsoleLog::~ConsoleLog() {
    Log::removeSink(sink);
}

void ConsoleLog::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    entries.clear();
    counts[0] = counts[1] = counts[2] = 0;
    revision++;
}

int ConsoleLog::count(LogLevel level) const {
    std::lock_guard<std::mutex> lock(mutex);
    return counts[(int)level];
}

uint64_t ConsoleLog::getRevision() const {
    std::lock_guard<std::mutex> lock(mutex);
    return revision;
}

bool ConsoleLog::getLast(ConsoleEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex);
    if (entries.empty()) return false;
    entry = entries.back();
    return true;
}
