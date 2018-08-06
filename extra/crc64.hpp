#pragma once

#include <cstdint>

uint64_t crc64(uint64_t crc, const uint8_t* data, uint64_t size);

inline
uint64_t crc64(const uint8_t* data, uint64_t size)
{
        return crc64(0, data, size);
}

uint64_t crc64_from_file(const char* filename);
