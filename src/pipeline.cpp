#include "pulseflow/pipeline.hpp"
#include <iostream>
#include <immintrin.h>

namespace pulseflow {

Pipeline::Pipeline(PipelineConfig config)
    : config_(std::move(config)),
      running_(true),
      start_time_(std::chrono::steady_clock::now())
{
    if (config_.enable_async) {
        worker_thread_ = std::thread(&Pipeline::worker_loop, this);
    }
}

Pipeline::~Pipeline() {
    drain_and_flush();
}

void Pipeline::add_sink(std::shared_ptr<ISink> sink) {
    if (sink) {
        sinks_.push_back(std::move(sink));
    }
}

bool Pipeline::matches_filter(const TelemetryEvent& event) const noexcept {
    if (event.level < config_.min_level) {
        return false;
    }
    if (!config_.filter_component.empty() && event.component != config_.filter_component) {
        return false;
    }
    if (!config_.filter_query.empty() && event.message.find(config_.filter_query) == std::string::npos) {
        return false;
    }
    return true;
}

bool Pipeline::ingest_line(std::string_view line) {
    ingested_count_.fetch_add(1, std::memory_order_relaxed);
    auto event_opt = LogParser::parse_line(line);
    if (!event_opt.has_value()) {
        return false;
    }
    return ingest_event(std::move(event_opt.value()));
}

bool Pipeline::ingest_event(TelemetryEvent&& event) {
    if (!matches_filter(event)) {
        filtered_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (!config_.enable_async) {
        // Synchronous dispatch
        for (auto& sink : sinks_) {
            sink->emit(event);
        }
        emitted_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // Asynchronous dispatch via lock-free ring buffer
    if (!ring_buffer_.try_push(std::move(event))) {
        dropped_count_.fetch_add(1, std::memory_order_relaxed);
        return false; // Ring buffer full
    }
    return true;
}

void Pipeline::worker_loop() {
    while (running_.load(std::memory_order_relaxed)) {
        auto item = ring_buffer_.try_pop();
        if (item.has_value()) {
            for (auto& sink : sinks_) {
                sink->emit(item.value());
            }
            emitted_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            // Backoff hint to CPU pipeline
#if defined(_MSC_VER) || defined(__GNUC__)
            _mm_pause();
#else
            std::this_thread::yield();
#endif
        }
    }

    // Drain remaining elements after stop request
    while (auto item = ring_buffer_.try_pop()) {
        for (auto& sink : sinks_) {
            sink->emit(item.value());
        }
        emitted_count_.fetch_add(1, std::memory_order_relaxed);
    }
}

void Pipeline::drain_and_flush() {
    if (running_.exchange(false)) {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    for (auto& sink : sinks_) {
        sink->flush();
    }
}

PipelineMetrics Pipeline::metrics() const noexcept {
    PipelineMetrics m;
    m.ingested_events = ingested_count_.load(std::memory_order_relaxed);
    m.emitted_events = emitted_count_.load(std::memory_order_relaxed);
    m.filtered_events = filtered_count_.load(std::memory_order_relaxed);
    m.dropped_events = dropped_count_.load(std::memory_order_relaxed);

    auto now = std::chrono::steady_clock::now();
    m.duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_);
    const double seconds = std::chrono::duration<double>(m.duration_ns).count();

    if (seconds > 0.0) {
        m.events_per_second = static_cast<double>(m.ingested_events) / seconds;
    }
    return m;
}

} // namespace pulseflow
