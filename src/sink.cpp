#include "pulseflow/sink.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace pulseflow {

namespace {

constexpr const char* COLOR_RESET   = "\033[0m";
constexpr const char* COLOR_BOLD    = "\033[1m";
constexpr const char* COLOR_RED     = "\033[31m";
constexpr const char* COLOR_GREEN   = "\033[32m";
constexpr const char* COLOR_YELLOW  = "\033[33m";
constexpr const char* COLOR_BLUE    = "\033[34m";
constexpr const char* COLOR_MAGENTA = "\033[35m";
constexpr const char* COLOR_CYAN    = "\033[36m";
constexpr const char* COLOR_DIM     = "\033[2m";

const char* level_color(LogLevel lvl) noexcept {
    switch (lvl) {
        case LogLevel::Trace: return "\033[2m";      // Dim
        case LogLevel::Debug: return "\033[36m";     // Cyan
        case LogLevel::Info:  return "\033[32m";     // Green
        case LogLevel::Warn:  return "\033[33m";     // Yellow
        case LogLevel::Error: return "\033[31m";     // Red
        case LogLevel::Fatal: return "\033[1;31m";   // Bold Red
        default:              return "";
    }
}

} // namespace

ConsoleSink::ConsoleSink(bool use_color) : use_color_(use_color) {}

void ConsoleSink::emit(const TelemetryEvent& event) {
    if (use_color_) {
        std::cout << level_color(event.level) << "[" << std::setw(5) << std::left << level_to_string(event.level) << "]" << COLOR_RESET << " ";
        if (!event.component.empty()) {
            std::cout << COLOR_CYAN << "[" << event.component << "]" << COLOR_RESET << " ";
        }
        std::cout << event.message;
        if (!event.attributes.empty()) {
            std::cout << " " << COLOR_DIM;
            for (const auto& [k, v] : event.attributes) {
                std::cout << k << "=" << v << " ";
            }
            std::cout << COLOR_RESET;
        }
        std::cout << "\n";
    } else {
        std::cout << "[" << std::setw(5) << std::left << level_to_string(event.level) << "] ";
        if (!event.component.empty()) {
            std::cout << "[" << event.component << "] ";
        }
        std::cout << event.message;
        for (const auto& [k, v] : event.attributes) {
            std::cout << " " << k << "=" << v;
        }
        std::cout << "\n";
    }
}

void ConsoleSink::flush() {
    std::cout.flush();
}

FileSink::FileSink(const std::string& path, std::size_t buffer_size)
    : buffer_(buffer_size) {
    out_.rdbuf()->pubsetbuf(buffer_.data(), buffer_.size());
    out_.open(path, std::ios::out | std::ios::app);
}

FileSink::~FileSink() {
    flush();
}

void FileSink::emit(const TelemetryEvent& event) {
    if (!out_) return;
    out_ << "[" << level_to_string(event.level) << "] ";
    if (!event.component.empty()) {
        out_ << "[" << event.component << "] ";
    }
    out_ << event.message;
    for (const auto& [k, v] : event.attributes) {
        out_ << " " << k << "=" << v;
    }
    out_ << "\n";
}

void FileSink::flush() {
    if (out_) {
        out_.flush();
    }
}

} // namespace pulseflow
