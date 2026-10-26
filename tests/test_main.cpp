#include "pulseflow/ring_buffer.hpp"
#include "pulseflow/event.hpp"
#include "pulseflow/parser.hpp"
#include "pulseflow/pipeline.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>
#include <vector>

namespace {

int g_tests_run = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        g_tests_run++; \
        std::cout << "[RUN] " << #fn << "...\n"; \
        fn(); \
    } while (0)

} // namespace

void test_ring_buffer_spsc() {
    pulseflow::SpscRingBuffer<int, 1024> rb;
    TEST_ASSERT(rb.empty(), "Buffer must initially be empty");
    TEST_ASSERT(rb.size() == 0, "Buffer size must be 0");

    // Sequential push and pop
    for (int i = 0; i < 100; ++i) {
        bool ok = rb.try_push(i);
        TEST_ASSERT(ok, "Push should succeed");
    }
    TEST_ASSERT(rb.size() == 100, "Size should be 100");

    for (int i = 0; i < 100; ++i) {
        auto val = rb.try_pop();
        TEST_ASSERT(val.has_value() && val.value() == i, "Popped value must match pushed order");
    }
    TEST_ASSERT(rb.empty(), "Buffer should be empty after popping all");

    // Concurrent producer-consumer stress test
    constexpr int TotalItems = 200'000;
    std::thread producer([&]() {
        for (int i = 0; i < TotalItems; ++i) {
            while (!rb.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::vector<int> received;
    received.reserve(TotalItems);

    std::thread consumer([&]() {
        while (static_cast<int>(received.size()) < TotalItems) {
            auto val = rb.try_pop();
            if (val.has_value()) {
                received.push_back(val.value());
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    TEST_ASSERT(received.size() == TotalItems, "All items must be received concurrently");
    for (int i = 0; i < TotalItems; ++i) {
        if (received[i] != i) {
            TEST_ASSERT(false, "Item sequence mismatch in concurrent ring buffer test");
        }
    }
}

void test_log_parser_json() {
    std::string json = "{\"level\":\"ERROR\",\"service\":\"payment\",\"msg\":\"Card authorization failed\",\"code\":402}";
    auto ev_opt = pulseflow::LogParser::parse_line(json);
    TEST_ASSERT(ev_opt.has_value(), "JSON line must parse successfully");
    const auto& ev = ev_opt.value();
    TEST_ASSERT(ev.level == pulseflow::LogLevel::Error, "Parsed level must be ERROR");
    TEST_ASSERT(ev.component == "payment", "Parsed component must be payment");
    TEST_ASSERT(ev.message == "Card authorization failed", "Parsed message must match");
}

void test_log_parser_key_value() {
    std::string kv = "level=WARN comp=auth user=\"admin_bob\" ip=192.168.1.1 msg=\"Invalid password attempt\"";
    auto ev_opt = pulseflow::LogParser::parse_line(kv);
    TEST_ASSERT(ev_opt.has_value(), "KV line must parse successfully");
    const auto& ev = ev_opt.value();
    TEST_ASSERT(ev.level == pulseflow::LogLevel::Warn, "Level must be WARN");
    TEST_ASSERT(ev.component == "auth", "Component must be auth");
    TEST_ASSERT(ev.message == "Invalid password attempt", "Message must match");
    TEST_ASSERT(ev.find_attribute("user") == "admin_bob", "Attribute 'user' must match");
    TEST_ASSERT(ev.find_attribute("ip") == "192.168.1.1", "Attribute 'ip' must match");
}

void test_line_iterator() {
    std::string sample = "Line 1\nLine 2\r\nLine 3\n";
    pulseflow::LineIterator iter(sample);

    TEST_ASSERT(iter.has_next(), "Iterator should have next");
    TEST_ASSERT(iter.next() == "Line 1", "Line 1 must match");
    TEST_ASSERT(iter.next() == "Line 2", "Line 2 must match without CRLF");
    TEST_ASSERT(iter.next() == "Line 3", "Line 3 must match");
    TEST_ASSERT(!iter.has_next(), "Iterator should be exhausted");
}

void test_pipeline_filtering() {
    pulseflow::PipelineConfig cfg;
    cfg.min_level = pulseflow::LogLevel::Warn;
    cfg.filter_component = "billing";
    cfg.enable_async = false;

    pulseflow::Pipeline pipe(cfg);
    auto sink = std::make_shared<pulseflow::NullSink>();
    pipe.add_sink(sink);

    // Should be filtered out by level (INFO < WARN)
    pipe.ingest_line("level=INFO component=billing msg=\"Test info\"");
    // Should be filtered out by component (security != billing)
    pipe.ingest_line("level=ERROR component=security msg=\"Test error\"");
    // Should pass
    pipe.ingest_line("level=WARN component=billing msg=\"Overdue invoice alert\"");

    pipe.drain_and_flush();
    auto m = pipe.metrics();
    TEST_ASSERT(m.ingested_events == 3, "Total ingested must be 3");
    TEST_ASSERT(m.filtered_events == 2, "Filtered must be 2");
    TEST_ASSERT(m.emitted_events == 1, "Emitted must be 1");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "       PulseFlow C++20 Test Suite       \n";
    std::cout << "========================================\n\n";

    RUN_TEST(test_ring_buffer_spsc);
    RUN_TEST(test_log_parser_json);
    RUN_TEST(test_log_parser_key_value);
    RUN_TEST(test_line_iterator);
    RUN_TEST(test_pipeline_filtering);

    std::cout << "\n----------------------------------------\n";
    std::cout << "Tests run: " << g_tests_run << " | Failures: " << g_tests_failed << "\n";
    if (g_tests_failed == 0) {
        std::cout << "ALL TESTS PASSED!\n";
        return 0;
    } else {
        std::cerr << "SOME TESTS FAILED!\n";
        return 1;
    }
}
