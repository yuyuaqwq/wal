//The MIT License(MIT)
//Copyright © 2025 https://github.com/yuyuaqwq
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <filesystem>
#include <fstream>
#include <optional>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif // NOMINMAX
# include <windows.h>
#else
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <fcntl.h>
#endif

#ifndef _WIN32
# define INVALID_HANDLE_VALUE -1
#endif // #ifndef _WIN32

namespace wal {

enum class access_mode {
    read,
    write,
    sync_needed,
};

enum class share_mode {
    shared,
    exclusive
};

#ifdef _WIN32
    using file_handle_type = HANDLE;
#else
    using file_handle_type = int;
#endif

const static file_handle_type invalid_handle = INVALID_HANDLE_VALUE;

namespace detail {

#ifdef _WIN32
namespace win {

/** Returns the 4 upper bytes of an 8-byte integer. */
inline DWORD int64_high(int64_t n) noexcept {
    return n >> 32;
}

/** Returns the 4 lower bytes of an 8-byte integer. */
inline DWORD int64_low(int64_t n) noexcept {
    return n & 0xffffffff;
}

inline file_handle_type open_file_helper(const std::filesystem::path& path, const access_mode mode) {
    DWORD flags = 0;
    if (mode == access_mode::sync_needed) {
        //flags |= FILE_FLAG_NO_BUFFERING;
        flags |= FILE_FLAG_WRITE_THROUGH;
    }
    return ::CreateFileW(path.wstring().c_str(),
            mode == access_mode::read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | flags,
            0);
}
} // win
#endif // _WIN32

/**
 * Returns the last platform specific system error (errno on POSIX and
 * GetLastError on Win) as a `std::error_code`.
 */
inline std::error_code last_error() noexcept {
    std::error_code error;
#ifdef _WIN32
    error.assign(GetLastError(), std::system_category());
#else
    error.assign(errno, std::system_category());
#endif
    return error;
}

inline file_handle_type open_file(const std::filesystem::path& path, const access_mode mode,
        std::error_code& error) {
    error.clear();
    
    if(path.empty()) {
        error = std::make_error_code(std::errc::invalid_argument);
        return invalid_handle;
    }
#ifdef _WIN32
    const auto handle = win::open_file_helper(path, mode);
#else // POSIX
    const auto handle = ::open(path.c_str(),
            (mode == access_mode::read ? O_RDONLY : O_RDWR) | O_CREAT);
#endif
    if(handle == invalid_handle) {
        error = detail::last_error();
    }
    return handle;
}

inline file_handle_type open_file(const std::filesystem::path& path, const access_mode mode) {
    std::error_code ec;
    auto handle = open_file(path, mode, ec);
    if (ec) {
        throw std::ios_base::failure{"tinyio::detail::win::open_file."};
    }
    return handle;
}


inline size_t query_file_size(file_handle_type handle, std::error_code& error) {
    error.clear();
#ifdef _WIN32
    LARGE_INTEGER file_size;
    if(::GetFileSizeEx(handle, &file_size) == 0) {
        error = detail::last_error();
        return 0;
    }
	return static_cast<int64_t>(file_size.QuadPart);
#else // POSIX
    struct stat sbuf;
    if(::fstat(handle, &sbuf) == -1) {
        error = detail::last_error();
        return 0;
    }
    return sbuf.st_size;
#endif
}

} // namespace detail

class file {
public:
    file() = default;
    ~file() {
        close();
    }

    file(const file&) = delete;
    void operator=(const file&) = delete;

    void open(const std::filesystem::path& path, const access_mode mode, std::error_code& error_code) {
        close();
        handle_ = detail::open_file(path, mode, error_code);
    }

    void open(const std::filesystem::path& path, const access_mode mode) {
        close();
        handle_ = detail::open_file(path, mode);
    }

    bool is_open() {
        return handle_ != invalid_handle;
    }

    void close() {
        if (handle_ != invalid_handle) {
#ifdef _WIN32
            ::CloseHandle(handle_);
#else // POSIX
            ::close(handle_);
#endif
            handle_ = invalid_handle;
        }
    }

    void seekg(uint64_t pos, std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        if (!::SetFilePointerEx(handle_, *reinterpret_cast<LARGE_INTEGER*>(&pos), NULL, FILE_BEGIN)) {
            error_code = detail::last_error();
        }
#else // POSIX
        auto cur_pos = ::lseek(handle_, pos, SEEK_SET);
        if (cur_pos == -1) {
            error_code = detail::last_error();
        }
#endif
    }

    void seekg(uint64_t pos) {
        std::error_code ec;
        seekg(pos, ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::seekg." };
        }
    }

    void seekg(uint64_t off, std::ios::seekdir dir, std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        DWORD mode;
        switch (dir) {
        case std::ios_base::beg:
            mode = FILE_BEGIN;
            break;
        case std::ios_base::cur:
            mode = FILE_CURRENT;
            break;
        case std::ios_base::end:
            mode = FILE_END;
            break;
        default:
            error_code = std::make_error_code(std::errc::invalid_argument);
            return;
        }
        if (!::SetFilePointerEx(handle_, *reinterpret_cast<LARGE_INTEGER*>(&off), NULL, mode)) {
            error_code = detail::last_error();
        }
#else // POSIX
        int mode;
        switch (dir) {
        case std::ios_base::beg:
            mode = SEEK_SET;
            break;
        case std::ios_base::cur:
            mode = SEEK_CUR;
            break;
        case std::ios_base::end:
            mode = SEEK_END;
            break;
        default:
            error_code = std::make_error_code(std::errc::invalid_argument);
            return;
        }
        auto cur_pos = ::lseek(handle_, off, mode);
        if (cur_pos == -1) {
            error_code = detail::last_error();
        }
#endif
    }

    void seekg(uint64_t off, std::ios::seekdir dir) {
        std::error_code ec;
        seekg(off, dir, ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::seekg" };
        }
    }

    uint64_t tellg(std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        LARGE_INTEGER liCurrentPosition;
        liCurrentPosition.QuadPart = 0;
        if (!SetFilePointerEx(handle_, liCurrentPosition, &liCurrentPosition, FILE_CURRENT)) {
            error_code = detail::last_error();
            return 0;
        }
        return liCurrentPosition.QuadPart;
#else // POSIX
        auto cur_pos = ::lseek(handle_, 0, SEEK_CUR);
        if (cur_pos == -1) {
            error_code = detail::last_error();
            return 0;
        }
        return cur_pos;
#endif
    }

    uint64_t tellg() {
        std::error_code ec;
        auto res = tellg(ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::tellg" };
        }
        return res;
    }

    uint64_t size(std::error_code& error_code) {
        return detail::query_file_size(handle_, error_code);
    }

    uint64_t size() {
        std::error_code ec;
        auto res = size(ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::size" };
        }
        return res;
    }

    void resize(uint64_t new_size, std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        LARGE_INTEGER large;
        large.QuadPart = new_size;
        if (::SetFilePointerEx(handle_, large, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
            error_code = detail::last_error();
        }
        if (!::SetEndOfFile(handle_)) {
            error_code = detail::last_error();
        }
#else // POSIX
        if (::ftruncate(handle_, new_size) == -1) {
            error_code = detail::last_error();
        }
#endif
    }

    void resize(uint64_t new_size) {
        std::error_code ec;
        resize(new_size, ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::resize" };
        }
    }

    size_t read(void* buf, size_t size, std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        DWORD read_size;
        const BOOL success = ::ReadFile(handle_, buf, size, &read_size, NULL);
        if (!success) {
            error_code = detail::last_error();
            return 0;
        }
        return read_size;
#else // POSIX
        auto read_size = ::read(handle_, buf, size);
        if (read_size < 0) {
            error_code = detail::last_error();
        }
        return read_size;
#endif
    }

    size_t read(void* buf, size_t size) {
        std::error_code ec;
        auto res = read(buf, size, ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::read" };
        }
        return res;
    }

    size_t write(const void* buf, size_t size, std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        DWORD write_len;
        if (!WriteFile(handle_, buf, size, &write_len, NULL)) {
            error_code = detail::last_error();
            return 0;
        }
        return write_len;
#else // POSIX
        auto write_len = ::write(handle_, buf, size);
        if (write_len <= 0) {
            error_code = detail::last_error();
            return 0;
        }
        return write_len;
#endif
    }

    size_t write(const void* buf, size_t size) {
        std::error_code ec;
        auto res = write(buf, size, ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::write" };
        }
        return res;
    }

    void sync(std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        if (!FlushFileBuffers(handle_)) {
            error_code = detail::last_error();
        }
#else // POSIX
        auto res = ::fsync(handle_);
        if (res == -1) {
            error_code = detail::last_error();
        }
#endif
    }

    void sync() {
        std::error_code ec;
        sync(ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::sync" };
        }
    }

    void lock(const share_mode& mode, std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        DWORD flags;
        switch (mode) {
        case share_mode::exclusive:
            flags = LOCKFILE_EXCLUSIVE_LOCK;
            break;
        case share_mode::shared:
            flags = 0;
            break;
        default:
            error_code = std::make_error_code(std::errc::invalid_argument);
            return;
        }
        ::OVERLAPPED overlapped{ 0 };
        if (!::LockFileEx(handle_, flags, 0, 1, 0, &overlapped)) {
            error_code = detail::last_error();
        }
#else // POSIX
        int operation;
        switch (mode) {
        case share_mode::exclusive:
            operation = LOCK_EX;
            break;
        case share_mode::shared:
            operation = LOCK_SH;
            break;
        default:
            error_code = std::make_error_code(std::errc::invalid_argument);
            return;
        }
        if (::flock(handle_, operation) < 0) {
            error_code = detail::last_error();
        }
#endif
    }

    void lock(const share_mode& mode) {
        std::error_code ec;
        lock(mode, ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::lock" };
        }
    }

    void unlock(std::error_code& error_code) {
        error_code.clear();
#ifdef _WIN32
        ::OVERLAPPED overlapped{ 0 };
        if (!::UnlockFileEx(handle_, 0, 1, 0, &overlapped)) {
            error_code = detail::last_error();
        }
#else // POSIX
        if (::flock(handle_, LOCK_UN) < 0) {
            error_code = detail::last_error();
        }
#endif
    }

    void unlock() {
        std::error_code ec;
        unlock(ec);
        if (ec) {
            throw std::ios_base::failure{ "tinyio::file::unlock" };
        }
    }

private:
    file_handle_type handle_{ invalid_handle };
};

} // namesapce wal