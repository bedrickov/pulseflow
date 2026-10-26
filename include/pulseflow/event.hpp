#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <chrono>

namespace pulseflow {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5
};

[[nodiscard]] constexpr std::string_view level_to_string(LogLevel lvl) noexcept {
    switch (lvl) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default:              return "UNKNOWN";
    }
}

[[nodiscard]] constexpr LogLevel string_to_level(std::string_view s) noexcept {
    if (s == "TRACE" || s == "trace") return LogLevel::Trace;
    if (s == "DEBUG" || s == "debug") return LogLevel::Debug;
    if (s == "INFO"  || s == "info")  return LogLevel::Info;
    if (s == "WARN"  || s == "warn" || s == "WARNING" || s == "warning")  return LogLevel::Warn;
    if (s == "ERROR" || s == "error" || s == "ERR" || s == "err") return LogLevel::Error;
    if (s == "FATAL" || s == "fatal") return LogLevel::Fatal;
    return LogLevel::Info;
}

struct EventAttribute {
    std::string key;
    std::string value;
};

struct TelemetryEvent {
    std::uint64_t timestamp_ns{0};
    LogLevel level{LogLevel::Info};
    std::string component;
    std::string message;
    std::vector<EventAttribute> attributes;

    [[nodiscard]] std::string_view find_attribute(std::string_view key) const noexcept {
        for (const auto& attr : attributes) {
            if (attr.key == key) return attr.value;
        }
        return {};
    }
};

} // namespace pulseflow
