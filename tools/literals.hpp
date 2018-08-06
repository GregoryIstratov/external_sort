#pragma once
#include <cstdint>

constexpr uint64_t operator "" _KiB (uint64_t x)
{
        return x * 1024;
}

constexpr uint64_t operator "" _MiB(uint64_t x)
{
        return x * 1024_KiB;
}

constexpr uint64_t operator "" _GiB(uint64_t x)
{
        return x * 1024_MiB;
}
