#pragma once

#include "pulseflow/event.hpp"
#include <memory>
#include <string>
#include <fstream>
#include <vector>

namespace pulseflow {

class ISink {
public:
    virtual ~ISink() = default;
    virtual void emit(const TelemetryEvent& event) = 0;
    virtual void flush() = 0;
};

class ConsoleSink : public ISink {
public:
    explicit ConsoleSink(bool use_color = true);
    void emit(const TelemetryEvent& event) override;
    void flush() override;

private:
    bool use_color_{true};
};

class FileSink : public ISink {
public:
    explicit FileSink(const std::string& path, std::size_t buffer_size = 65536);
    ~FileSink() override;

    void emit(const TelemetryEvent& event) override;
    void flush() override;

private:
    std::ofstream out_;
    std::vector<char> buffer_;
};

class NullSink : public ISink {
public:
    void emit(const TelemetryEvent&) override {}
    void flush() override {}
};

} // namespace pulseflow
