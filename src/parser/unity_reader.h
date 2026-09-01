#ifndef UNITY_READER_H
#define UNITY_READER_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

class UnityReader {
private:
    const uint8_t* data;
    size_t size;
    size_t position;
    bool has_error;

public:
    UnityReader(const uint8_t* buffer, size_t buf_size) 
        : data(buffer), size(buf_size), position(0), has_error(false) {}

    size_t get_position() const { return position; }
    bool is_eof() const { return position >= size; }
    bool failed() const { return has_error; }

    void set_position(size_t pos) {
        if (pos > size) {
            has_error = true;
            return;
        }
        position = pos;
    }

    void align(size_t n = 4) {
        size_t rem = position % n;
        if (rem != 0) {
            size_t needed = n - rem;
            if (position + needed > size) {
                has_error = true;
                return;
            }
            position += needed;
        }
    }

    // 8-bit
    uint8_t read_u8() {
        if (position + 1 > size) {
            has_error = true;
            return 0;
        }
        return data[position++];
    }

    int8_t read_i8() {
        return static_cast<int8_t>(read_u8());
    }

    // 16-bit Little Endian
    uint16_t read_u16_le() {
        if (position + 2 > size) {
            has_error = true;
            return 0;
        }
        uint16_t val = 0;
        std::memcpy(&val, data + position, 2);
        position += 2;
        return val;
    }

    int16_t read_i16_le() {
        return static_cast<int16_t>(read_u16_le());
    }

    // 16-bit Big Endian
    uint16_t read_u16_be() {
        if (position + 2 > size) {
            has_error = true;
            return 0;
        }
        uint16_t val = (static_cast<uint16_t>(data[position]) << 8) | static_cast<uint16_t>(data[position + 1]);
        position += 2;
        return val;
    }

    int16_t read_i16_be() {
        return static_cast<int16_t>(read_u16_be());
    }

    // 32-bit Little Endian
    uint32_t read_u32_le() {
        if (position + 4 > size) {
            has_error = true;
            return 0;
        }
        uint32_t val = 0;
        std::memcpy(&val, data + position, 4);
        position += 4;
        return val;
    }

    int32_t read_i32_le() {
        return static_cast<int32_t>(read_u32_le());
    }

    // 32-bit Big Endian
    uint32_t read_u32_be() {
        if (position + 4 > size) {
            has_error = true;
            return 0;
        }
        uint32_t val = (static_cast<uint32_t>(data[position]) << 24) |
                       (static_cast<uint32_t>(data[position + 1]) << 16) |
                       (static_cast<uint32_t>(data[position + 2]) << 8) |
                       static_cast<uint32_t>(data[position + 3]);
        position += 4;
        return val;
    }

    int32_t read_i32_be() { 
        return static_cast<int32_t>(read_u32_be()); 
    }

    // 64-bit Little Endian
    uint64_t read_u64_le() {
        if (position + 8 > size) {
            has_error = true;
            return 0;
        }
        uint64_t val = 0;
        std::memcpy(&val, data + position, 8);
        position += 8;
        return val;
    }

    int64_t read_i64_le() {
        return static_cast<int64_t>(read_u64_le());
    }

    // 64-bit Big Endian
    uint64_t read_u64_be() {
        if (position + 8 > size) {
            has_error = true;
            return 0;
        }
        uint64_t val = 0;
        for (int i = 0; i < 8; ++i) {
            val = (val << 8) | data[position + i];
        }
        position += 8;
        return val;
    }

    int64_t read_i64_be() {
        return static_cast<int64_t>(read_u64_be());
    }

    // Floats
    float read_float_le() {
        if (position + 4 > size) {
            has_error = true;
            return 0.0f;
        }
        float val = 0.0f;
        std::memcpy(&val, data + position, 4);
        position += 4;
        return val;
    }

    // Strings
    std::string read_string_null() {
        std::string res;
        while (position < size) {
            char c = static_cast<char>(data[position++]);
            if (c == '\0') break;
            res += c;
        }
        return res;
    }

    std::string read_string_aligned() {
        uint32_t len = read_u32_le();
        if (failed() || position + len > size) {
            has_error = true;
            return "";
        }
        std::string res(reinterpret_cast<const char*>(data + position), len);
        position += len;
        align(4);
        return res;
    }

    // Byte Arrays
    std::vector<uint8_t> read_bytes(size_t count) {
        if (position + count > size) {
            has_error = true;
            return {};
        }
        std::vector<uint8_t> bytes(data + position, data + position + count);
        position += count;
        return bytes;
    }
};

#endif // UNITY_READER_H
