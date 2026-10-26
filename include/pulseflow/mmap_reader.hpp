#pragma once

#include <string>
#include <string_view>
#include <span>
#include <cstddef>
#include <cstdint>

namespace pulseflow {

class MmapReader {
public:
    MmapReader() = default;
    explicit MmapReader(const std::string& path);
    ~MmapReader();

    MmapReader(const MmapReader&) = delete;
    MmapReader& operator=(const MmapReader&) = delete;
    MmapReader(MmapReader&& other) noexcept;
    MmapReader& operator=(MmapReader&& other) noexcept;

    bool open(const std::string& path);
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return data_ != nullptr; }
    [[nodiscard]] const char* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::string_view view() const noexcept { return {data_, size_}; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    std::string path_;
    const char* data_{nullptr};
    std::size_t size_{0};

#ifdef _WIN32
    void* file_handle_{nullptr};
    void* mapping_handle_{nullptr};
#else
    int fd_{-1};
#endif
};

} // namespace pulseflow
