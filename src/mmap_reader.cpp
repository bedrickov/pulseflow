#include "pulseflow/mmap_reader.hpp"
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pulseflow {

MmapReader::MmapReader(const std::string& path) {
    open(path);
}

MmapReader::~MmapReader() {
    close();
}

MmapReader::MmapReader(MmapReader&& other) noexcept
    : path_(std::move(other.path_)),
      data_(other.data_),
      size_(other.size_)
#ifdef _WIN32
      , file_handle_(other.file_handle_),
      mapping_handle_(other.mapping_handle_)
#else
      , fd_(other.fd_)
#endif
{
    other.data_ = nullptr;
    other.size_ = 0;
#ifdef _WIN32
    other.file_handle_ = nullptr;
    other.mapping_handle_ = nullptr;
#else
    other.fd_ = -1;
#endif
}

MmapReader& MmapReader::operator=(MmapReader&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        data_ = other.data_;
        size_ = other.size_;
#ifdef _WIN32
        file_handle_ = other.file_handle_;
        mapping_handle_ = other.mapping_handle_;
        other.file_handle_ = nullptr;
        other.mapping_handle_ = nullptr;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

#ifdef _WIN32
bool MmapReader::open(const std::string& path) {
    close();
    path_ = path;

    // Convert UTF-8 path to wide
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) return false;
    std::wstring wide_path(wide_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide_path.data(), wide_len);

    file_handle_ = CreateFileW(
        wide_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (file_handle_ == INVALID_HANDLE_VALUE) {
        file_handle_ = nullptr;
        return false;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle_, &file_size) || file_size.QuadPart == 0) {
        CloseHandle(file_handle_);
        file_handle_ = nullptr;
        return false;
    }

    size_ = static_cast<std::size_t>(file_size.QuadPart);

    mapping_handle_ = CreateFileMappingW(
        file_handle_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );

    if (!mapping_handle_) {
        CloseHandle(file_handle_);
        file_handle_ = nullptr;
        return false;
    }

    data_ = static_cast<const char*>(MapViewOfFile(
        mapping_handle_,
        FILE_MAP_READ,
        0,
        0,
        size_
    ));

    if (!data_) {
        CloseHandle(mapping_handle_);
        CloseHandle(file_handle_);
        mapping_handle_ = nullptr;
        file_handle_ = nullptr;
        return false;
    }

    return true;
}

void MmapReader::close() noexcept {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_handle_) {
        CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    if (file_handle_) {
        CloseHandle(file_handle_);
        file_handle_ = nullptr;
    }
    size_ = 0;
}

#else

bool MmapReader::open(const std::string& path) {
    close();
    path_ = path;

    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) return false;

    struct stat st;
    if (fstat(fd_, &st) < 0 || st.st_size == 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    size_ = static_cast<std::size_t>(st.st_size);
    void* ptr = ::mmap(nullptr, size_, PROT_READ, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    data_ = static_cast<const char*>(ptr);
    madvise(const_cast<char*>(data_), size_, MADV_SEQUENTIAL);
    return true;
}

void MmapReader::close() noexcept {
    if (data_) {
        ::munmap(const_cast<char*>(data_), size_);
        data_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    size_ = 0;
}

#endif

} // namespace pulseflow
