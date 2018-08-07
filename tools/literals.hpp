#pragma once
#include <cstdint>

constexpr unsigned long long int operator "" _KiB( unsigned long long int v )
{
        return v * 1024;
}

constexpr unsigned long long int operator "" _MiB( unsigned long long int x )
{
        return x * 1024_KiB;
}

constexpr unsigned long long int operator "" _GiB( unsigned long long int x )
{
        return x * 1024_MiB;
}
