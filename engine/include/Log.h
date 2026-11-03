#pragma once
#include <functional>
#include <string>

enum class LogLevel { Info, Warning, Error };

// Process-wide log stream. It is one of the few intentional globals: messages come from
// deep inside resource constructors that have no reason to know about the Editor. The
// Editor's console listens through a sink instead of the engine depending on it.
namespace Log {
    using Sink = std::function<void(LogLevel level, const char* category, const std::string& message)>;

    int addSink(Sink sink);
    void removeSink(int handle);

    void write(LogLevel level, const char* category, const std::string& message);

    inline void info(const char* category, const std::string& message) {
        write(LogLevel::Info, category, message);
    }
    inline void warn(const char* category, const std::string& message) {
        write(LogLevel::Warning, category, message);
    }
    inline void error(const char* category, const std::string& message) {
        write(LogLevel::Error, category, message);
    }
}
