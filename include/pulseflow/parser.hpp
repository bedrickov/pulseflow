#pragma once

#include "pulseflow/event.hpp"
#include <string_view>
#include <vector>
#include <optional>

namespace pulseflow {

class LogParser {
public:
    /// Parse a single line of log text (auto-detects JSON or Logfmt/Key-Value)
    [[nodiscard]] static std::optional<TelemetryEvent> parse_line(std::string_view line);

    /// Parse a JSON formatted log line
    [[nodiscard]] static std::optional<TelemetryEvent> parse_json(std::string_view line);

    /// Parse a Key-Value / Logfmt formatted log line
    [[nodiscard]] static std::optional<TelemetryEvent> parse_key_value(std::string_view line);

private:
    static std::string_view trim(std::string_view s) noexcept;
    static std::string_view extract_json_field(std::string_view json, std::string_view key) noexcept;
};

class LineIterator {
public:
    explicit LineIterator(std::string_view text) : text_(text), pos_(0) {}

    [[nodiscard]] bool has_next() const noexcept {
        return pos_ < text_.size();
    }

    std::string_view next() noexcept {
        if (pos_ >= text_.size()) return {};
        const std::size_t start = pos_;
        const std::size_t newline = text_.find('\n', pos_);

        if (newline == std::string_view::npos) {
            pos_ = text_.size();
            std::string_view line = text_.substr(start);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            return line;
        }

        pos_ = newline + 1;
        std::size_t len = newline - start;
        if (len > 0 && text_[start + len - 1] == '\r') {
            --len;
        }
        return text_.substr(start, len);
    }

private:
    std::string_view text_;
    std::size_t pos_{0};
};

} // namespace pulseflow
