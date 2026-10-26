#pragma once

#include "pulseflow/event.hpp"
#include "pulseflow/ring_buffer.hpp"
#include "pulseflow/sink.hpp"
#include "pulseflow/parser.hpp"
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

namespace pulseflow {

struct PipelineConfig {
    LogLevel min_level{LogLevel::Trace};
    std::string filter_component;
    std::string filter_query;
    bool enable_async{true};
};

struct PipelineMetrics {
    std::size_t ingested_events{0};
    std::size_t emitted_events{0};
    std::size_t filtered_events{0};
    std::size_t dropped_events{0};
    std::chrono::nanoseconds duration_ns{0};
    double events_per_second{0.0};
};

class Pipeline {
public:
    explicit Pipeline(PipelineConfig config = {});
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void add_sink(std::shared_ptr<ISink> sink);

    /// Ingest a single raw line of text
    bool ingest_line(std::string_view line);

    /// Ingest an already parsed event
    bool ingest_event(TelemetryEvent&& event);

    /// Flush all buffers and wait for consumer worker to drain
    void drain_and_flush();

    /// Get live performance metrics
    [[nodiscard]] PipelineMetrics metrics() const noexcept;

private:
    void worker_loop();
    [[nodiscard]] bool matches_filter(const TelemetryEvent& event) const noexcept;

    PipelineConfig config_;
    std::vector<std::shared_ptr<ISink>> sinks_;
    SpscRingBuffer<TelemetryEvent, 65536> ring_buffer_;

    std::atomic<bool> running_{false};
    std::thread worker_thread_;

    std::atomic<std::size_t> ingested_count_{0};
    std::atomic<std::size_t> emitted_count_{0};
    std::atomic<std::size_t> filtered_count_{0};
    std::atomic<std::size_t> dropped_count_{0};
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace pulseflow
