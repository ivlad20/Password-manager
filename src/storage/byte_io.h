#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// Little-endian helpers used by the vault file format and the entry serializer.
// The byte order is fixed so a vault written on one machine opens on any other.

inline void append_u32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

inline void append_u64(std::vector<uint8_t>& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

inline void append_bytes(std::vector<uint8_t>& out, const uint8_t* data, size_t size) {
    out.insert(out.end(), data, data + size);
}

// Length-prefixed string: u32 length followed by the raw bytes.
inline void append_string(std::vector<uint8_t>& out, const std::string& value) {
    if (value.size() > UINT32_MAX) {
        throw std::runtime_error("string too long to serialize");
    }
    append_u32(out, static_cast<uint32_t>(value.size()));
    append_bytes(out, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

// Reads values sequentially from a buffer and throws if the buffer ends early,
// so a truncated or corrupted file can never cause an out-of-bounds read.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : data(data), size(size) {}

    uint32_t read_u32() {
        require(4);
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= static_cast<uint32_t>(data[pos + i]) << (8 * i);
        }
        pos += 4;
        return value;
    }

    uint64_t read_u64() {
        require(8);
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(data[pos + i]) << (8 * i);
        }
        pos += 8;
        return value;
    }

    void read_bytes(uint8_t* out, size_t count) {
        require(count);
        std::memcpy(out, data + pos, count);
        pos += count;
    }

    std::vector<uint8_t> read_rest() {
        std::vector<uint8_t> rest(data + pos, data + size);
        pos = size;
        return rest;
    }

    std::string read_string() {
        uint32_t length = read_u32();
        require(length);
        std::string value(reinterpret_cast<const char*>(data + pos), length);
        pos += length;
        return value;
    }

    [[nodiscard]] size_t remaining() const { return size - pos; }

private:
    void require(size_t count) const {
        if (count > size - pos) {
            throw std::runtime_error("unexpected end of data");
        }
    }

    const uint8_t* data;
    size_t size;
    size_t pos = 0;
};
