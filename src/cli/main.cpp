#include "pulseflow/event.hpp"
#include "pulseflow/ring_buffer.hpp"
#include "pulseflow/mmap_reader.hpp"
#include "pulseflow/parser.hpp"
#include "pulseflow/pipeline.hpp"
#include "pulseflow/sink.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <immintrin.h>

namespace {

void print_banner() {
    std::cout << "\033[36m\033[1m";
    std::cout << R"(
    ____        __          ________                 
   / __ \__  __/ /___ ___  / ____/ /___ _      __    
  / /_/ / / / / / __ `/ _ \/ /_  / / __ \ | /| / /    
 / ____/ /_/ / (__  )  __/ __/ / / /_/ / |/ |/ /     
/_/    \__,_/_/____/\___/_/   /_/\____/|__/|__/      
)";
    std::cout << "\033[0m";
    std::cout << " Ultra-Low-Latency Zero-Copy Async Telemetry Pipeline (C++20)\n";
    std::cout << " Lock-Free SPSC Ring Buffer • Memory Mapped I/O • SIMD-Ready Structured Parser\n\n";
}

void print_help() {
    print_banner();
    std::cout << R"(Usage:
  pulseflow stream --file <path> [options]
  pulseflow bench [--events <N>] [--sync]
  pulseflow parse "<log_string>"
  pulseflow --help

Options:
  --file <path>          Path to input log file (Mmap zero-copy streaming)
  --level <LEVEL>        Filter by minimum level: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
  --component <name>     Filter events by component name
  --query <substring>    Filter events containing search substring
  --out <file_path>      Output to target file sink in addition to console
  --events <count>       Number of events for throughput benchmark (default: 5,000,000)
  --sync                 Run in synchronous mode (bypassing ring buffer)
  --no-color             Disable ANSI coloring
  -h, --help             Show this help screen
)";
}

void run_benchmark(std::size_t target_events, bool is_sync) {
    print_banner();
    std::cout << "=== Running Microbenchmark: " << target_events << " Events (" << (is_sync ? "Synchronous" : "Async Lock-Free RingBuffer") << ") ===\n\n";

    pulseflow::PipelineConfig config;
    config.enable_async = !is_sync;

    pulseflow::Pipeline pipeline(config);
    pipeline.add_sink(std::make_shared<pulseflow::NullSink>());

    const std::string sample_log = "level=INFO component=payment_gateway user_id=98124 status=success duration_ms=14.2 msg=\"Transaction authorized successfully\"";

    std::cout << "[*] Warmup and throughput measuring in progress...\n";
    auto t0 = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < target_events; ++i) {
        while (!pipeline.ingest_line(sample_log)) {
            // Backoff if ring buffer is momentarily full
#if defined(_MSC_VER) || defined(__GNUC__)
            _mm_pause();
#else
            std::this_thread::yield();
#endif
        }
    }

    pipeline.drain_and_flush();
    auto t1 = std::chrono::steady_clock::now();

    const double elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
    const double mops = (static_cast<double>(target_events) / elapsed_sec) / 1'000'000.0;
    const double ns_per_event = (elapsed_sec * 1'000'000'000.0) / static_cast<double>(target_events);

    std::cout << "\n=======================================================\n";
    std::cout << " BENCHMARK RESULTS\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << "  Total Events Ingested : " << target_events << "\n";
    std::cout << "  Total Time Elapsed    : " << std::fixed << std::setprecision(4) << elapsed_sec << " s\n";
    std::cout << "  Throughput Rate       : \033[32m\033[1m" << std::fixed << std::setprecision(2) << mops << " Million events/sec\033[0m\n";
    std::cout << "  Avg Ingestion Latency : \033[36m" << std::fixed << std::setprecision(1) << ns_per_event << " ns/event\033[0m\n";
    std::cout << "=======================================================\n\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string command = argv[1];
    if (command == "--help" || command == "-h" || command == "help") {
        print_help();
        return 0;
    }

    if (command == "bench") {
        std::size_t events = 3'000'000;
        bool is_sync = false;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--events" && i + 1 < argc) {
                events = std::stoull(argv[++i]);
            } else if (arg == "--sync") {
                is_sync = true;
            }
        }
        run_benchmark(events, is_sync);
        return 0;
    }

    if (command == "parse") {
        if (argc < 3) {
            std::cerr << "[-] Error: provide a log line to parse.\n";
            return 1;
        }
        std::string raw_line = argv[2];
        auto event_opt = pulseflow::LogParser::parse_line(raw_line);
        if (!event_opt.has_value()) {
            std::cerr << "[-] Failed to parse log line.\n";
            return 1;
        }

        print_banner();
        const auto& ev = event_opt.value();
        std::cout << "[Structured Event Parsed]\n";
        std::cout << "  Timestamp : " << ev.timestamp_ns << " ns\n";
        std::cout << "  Level     : " << pulseflow::level_to_string(ev.level) << "\n";
        std::cout << "  Component : " << (ev.component.empty() ? "(none)" : ev.component) << "\n";
        std::cout << "  Message   : " << ev.message << "\n";
        if (!ev.attributes.empty()) {
            std::cout << "  Attributes:\n";
            for (const auto& [k, v] : ev.attributes) {
                std::cout << "    - " << k << " = " << v << "\n";
            }
        }
        std::cout << "\n";
        return 0;
    }

    if (command == "stream") {
        std::string file_path;
        pulseflow::PipelineConfig config;
        std::string out_file;
        bool use_color = true;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--file" && i + 1 < argc) {
                file_path = argv[++i];
            } else if (arg == "--level" && i + 1 < argc) {
                config.min_level = pulseflow::string_to_level(argv[++i]);
            } else if (arg == "--component" && i + 1 < argc) {
                config.filter_component = argv[++i];
            } else if (arg == "--query" && i + 1 < argc) {
                config.filter_query = argv[++i];
            } else if (arg == "--out" && i + 1 < argc) {
                out_file = argv[++i];
            } else if (arg == "--no-color") {
                use_color = false;
            } else if (arg == "--sync") {
                config.enable_async = false;
            }
        }

        if (file_path.empty()) {
            std::cerr << "[-] Error: --file <path> is required for stream command.\n";
            return 1;
        }

        print_banner();
        std::cout << "[*] Opening file with zero-copy Memory-Mapped I/O: " << file_path << "...\n";
        pulseflow::MmapReader mmap(file_path);
        if (!mmap.is_open()) {
            std::cerr << "[-] Error: Failed to memory map file (file not found or access denied).\n";
            return 2;
        }

        std::cout << "[+] Successfully mapped " << mmap.size() << " bytes into virtual address space.\n";
        pulseflow::Pipeline pipeline(config);
        pipeline.add_sink(std::make_shared<pulseflow::ConsoleSink>(use_color));
        if (!out_file.empty()) {
            pipeline.add_sink(std::make_shared<pulseflow::FileSink>(out_file));
            std::cout << "[+] Attached FileSink output to: " << out_file << "\n";
        }

        pulseflow::LineIterator iter(mmap.view());
        while (iter.has_next()) {
            std::string_view line = iter.next();
            if (!line.empty()) {
                pipeline.ingest_line(line);
            }
        }

        pipeline.drain_and_flush();
        auto metrics = pipeline.metrics();
        std::cout << "\n[Stream Completed]\n";
        std::cout << "  Ingested Events : " << metrics.ingested_events << "\n";
        std::cout << "  Emitted Events  : " << metrics.emitted_events << "\n";
        std::cout << "  Filtered Out    : " << metrics.filtered_events << "\n";
        std::cout << "  Throughput Rate : " << std::fixed << std::setprecision(1) << metrics.events_per_second << " events/sec\n\n";
        return 0;
    }

    print_help();
    return 0;
}
