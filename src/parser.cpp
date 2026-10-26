#include "pulseflow/parser.hpp"
#include <cctype>
#include <algorithm>

namespace pulseflow {

std::string_view LogParser::trim(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

std::string_view LogParser::extract_json_field(std::string_view json, std::string_view key) noexcept {
    // Search for "key": or "key" :
    std::string pattern = "\"" + std::string(key) + "\"";
    std::size_t pos = json.find(pattern);
    if (pos == std::string_view::npos) return {};

    pos += pattern.size();
    while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ':')) {
        ++pos;
    }
    if (pos >= json.size()) return {};

    if (json[pos] == '"') {
        // Quoted string value (handle escaped quotes properly)
        ++pos;
        std::size_t end = pos;
        while (end < json.size()) {
            if (json[end] == '"' && (end == pos || json[end - 1] != '\\')) {
                break;
            }
            ++end;
        }
        if (end >= json.size()) return {};
        return json.substr(pos, end - pos);
    } else {
        // Numeric, boolean or literal value
        std::size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && !std::isspace(static_cast<unsigned char>(json[end]))) {
            ++end;
        }
        return json.substr(pos, end - pos);
    }
}

std::optional<TelemetryEvent> LogParser::parse_json(std::string_view line) {
    line = trim(line);
    if (line.empty() || line.front() != '{' || line.back() != '}') {
        return std::nullopt;
    }

    TelemetryEvent event;
    event.timestamp_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    // Extract level
    auto lvl = extract_json_field(line, "level");
    if (lvl.empty()) lvl = extract_json_field(line, "lvl");
    if (lvl.empty()) lvl = extract_json_field(line, "severity");
    if (!lvl.empty()) {
        event.level = string_to_level(lvl);
    }

    // Extract message
    auto msg = extract_json_field(line, "message");
    if (msg.empty()) msg = extract_json_field(line, "msg");
    if (msg.empty()) msg = extract_json_field(line, "log");
    event.message = std::string(msg);

    // Extract component / logger
    auto comp = extract_json_field(line, "component");
    if (comp.empty()) comp = extract_json_field(line, "logger");
    if (comp.empty()) comp = extract_json_field(line, "service");
    event.component = std::string(comp);

    return event;
}

std::optional<TelemetryEvent> LogParser::parse_key_value(std::string_view line) {
    line = trim(line);
    if (line.empty()) return std::nullopt;

    TelemetryEvent event;
    event.timestamp_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i >= line.size()) break;

        std::size_t key_start = i;
        while (i < line.size() && line[i] != '=' && !std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }

        std::string_view key = line.substr(key_start, i - key_start);
        if (i >= line.size() || line[i] != '=') {
            // Standalone word, treat as part of message if message is empty
            if (event.message.empty()) {
                event.message = std::string(key);
            } else {
                event.message += " " + std::string(key);
            }
            continue;
        }

        ++i; // skip '='
        if (i >= line.size()) break;

        std::string_view val;
        if (line[i] == '"') {
            ++i;
            std::size_t val_start = i;
            while (i < line.size()) {
                if (line[i] == '"' && (i == val_start || line[i - 1] != '\\')) {
                    break;
                }
                ++i;
            }
            val = line.substr(val_start, i - val_start);
            if (i < line.size() && line[i] == '"') ++i;
        } else {
            std::size_t val_start = i;
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            val = line.substr(val_start, i - val_start);
        }

        // Helper to unescape \" -> "
        auto unescape = [](std::string_view sv) {
            std::string out;
            out.reserve(sv.size());
            for (std::size_t k = 0; k < sv.size(); ++k) {
                if (sv[k] == '\\' && k + 1 < sv.size() && sv[k + 1] == '"') {
                    out.push_back('"');
                    ++k;
                } else {
                    out.push_back(sv[k]);
                }
            }
            return out;
        };

        if (key == "level" || key == "lvl") {
            event.level = string_to_level(val);
        } else if (key == "msg" || key == "message") {
            event.message = unescape(val);
        } else if (key == "component" || key == "comp" || key == "service") {
            event.component = unescape(val);
        } else {
            event.attributes.push_back({std::string(key), unescape(val)});
        }
    }

    return event;
}

std::optional<TelemetryEvent> LogParser::parse_line(std::string_view line) {
    line = trim(line);
    if (line.empty()) return std::nullopt;

    if (line.front() == '{' && line.back() == '}') {
        return parse_json(line);
    }
    return parse_key_value(line);
}

} // namespace pulseflow
