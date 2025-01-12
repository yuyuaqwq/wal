//The MIT License(MIT)
//Copyright © 2025 https://github.com/yuyuaqwq
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "wal/log_writer.h"

#include <cstring>
#include <string_view>

#include "crc32.h"

namespace wal {

Writer::Writer() = default;

Writer::~Writer() = default;

void Writer::Open(std::string_view path, tinyio::access_mode access_mode) {
    file_.open(path, access_mode);
    file_.seekg(0);
    rep_.reserve(kBlockSize);
}

void Writer::Close() {
    file_.close();
    block_offset_ = 0;
    size_ = 0;
}

void Writer::AppendRecordToBuffer(std::span<const uint8_t> data) {
    auto size = kHeaderSize + data.size();
    size_ += size;
    if (block_offset_ + size > kBlockSize) {
        FlushBuffer();
        AppendRecord(data);
        return;
    }
    LogRecord record {
        .type = RecordType::kFullType,
    };
    record.size = static_cast<uint16_t>(data.size());

    Crc32 crc32;
    crc32.Append(&record.size, kHeaderSize - sizeof(record.checksum));
    crc32.Append(data.data(), data.size());
    record.checksum = crc32.End();

    auto raw_size = rep_.size();
    rep_.resize(raw_size + kHeaderSize);
    std::memcpy(&rep_[raw_size], &record, kHeaderSize);
    raw_size = rep_.size();
    if (data.size() > 0) {
        rep_.resize(raw_size + data.size());
        std::memcpy(&rep_[raw_size], data.data(), data.size());
    }
    block_offset_ += size;
}

void Writer::AppendRecordToBuffer(std::string_view data) {
    AppendRecordToBuffer({ reinterpret_cast<const uint8_t*>(data.data()), data.size() });
}

void Writer::FlushBuffer() {
    if (rep_.size() > 0) {
        file_.write(rep_.data(), rep_.size());
        rep_.resize(0);
    }
}

void Writer::Sync() {
    file_.sync();
}

void Writer::AppendRecord(std::span<const uint8_t> data) {
    assert(block_offset_ <= kBlockSize);
    auto ptr = data.data();
    auto left = data.size();
    bool begin = true;
    do {
        const size_t leftover = kBlockSize - block_offset_;
        if (leftover < kHeaderSize) {
            if (leftover > 0) {
                file_.write(kBlockPadding, leftover);
            }
            block_offset_ = 0;
        }

        const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
        const size_t fragment_size = (left < avail) ? left : avail;

        RecordType type;
        const bool end = (left == fragment_size);
        if (begin && end) {
            type = RecordType::kFullType;
        } else if (begin) {
            type = RecordType::kFirstType;
        } else if (end) {
            type = RecordType::kLastType;
        } else {
            type = RecordType::kMiddleType;
        }

        EmitPhysicalRecord(type, ptr, fragment_size);
        ptr += fragment_size;
        left -= fragment_size;
        begin = false;
    } while (left > 0);
}

void Writer::EmitPhysicalRecord(RecordType type, const uint8_t* ptr, size_t size) {
    assert(size < 0xffff);
    assert(block_offset_ + kHeaderSize + size <= kBlockSize);

    uint8_t buf[kHeaderSize];
    auto record = reinterpret_cast<LogRecord*>(buf);
    record->type = type;
    record->size = static_cast<uint16_t>(size);

    Crc32 crc32;
    crc32.Append(&record->size, kHeaderSize - sizeof(record->checksum));
    crc32.Append(ptr, size);
    record->checksum = crc32.End();

    file_.write(buf, kHeaderSize);
    file_.write(ptr, size);
    block_offset_ += kHeaderSize + size;
}

} // namespace wal