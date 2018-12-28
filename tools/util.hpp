#pragma once
#include <type_traits>
#include <thread>
#include <iomanip>
#include <sstream>
#include <cstring>

template<typename T>
constexpr void set_flag(T* x, T flag)
{
        static_assert(std::is_enum<T>::value || std::is_trivial<T>::value,
                "T must be enum or trivial");

        *x = *x | flag;
}

template<typename T>
constexpr bool test_flag(T x, T flag)
{
        static_assert(std::is_enum<T>::value || std::is_trivial<T>::value,
                "T must be enum or trivial");

        return static_cast<bool>(x & flag);
}

template<typename T>
constexpr void clear_flag(T* x, T flag)
{
        static_assert(std::is_enum<T>::value || std::is_trivial<T>::value,
                "T must be enum or trivial");

        *x = *x & (~flag);
}

template<typename T>
constexpr bool test_and_clear(T* var, T flag)
{
        if ((*var) & flag)
        {
                (*var) &= ~flag;
                return true;
        }

        return false;
}

inline
std::string get_thread_id_str()
{
        static const auto flags = std::ios_base::hex
                                | std::ios_base::uppercase 
                                | std::ios_base::showbase;

        std::stringstream ss;
        ss << std::resetiosflags(std::ios_base::dec)
                << std::setiosflags(flags)
                << std::this_thread::get_id();

        return ss.str();
}

template<typename T>
T zero_move(T& o) noexcept
{
        static_assert(std::is_trivial<T>::value, "Type must be trivial");

        T tmp = o;
        o = T();

        return tmp;
}

template<typename A, typename B>
A div_up(A a, B b)
{
        static_assert(std::is_integral<A>::value,
                      "A must be of an integral type");

        static_assert(std::is_integral<B>::value,
                      "B must be of an integral type");

        return (a + b - 1) / b;
}

template<typename A, typename B>
A round_up(A i, B mod)
{
        static_assert(std::is_integral<A>::value,
                      "A must be of an integral type");

        static_assert(std::is_integral<B>::value,
                      "B must be of an integral type");

        return ((i + mod - 1) / mod) * mod;
}

template<typename A, typename B>
A round_down(A i, B mod)
{
        static_assert(std::is_integral<A>::value,
                      "A must be of an integral type");

        static_assert(std::is_integral<B>::value,
                      "B must be of an integral type");

        return div_up(i - mod + 1, mod) * mod;
}

/*
// 30-50% faster than std::memcpy
inline void mem_copy(void* dst, const void* src, std::size_t size) noexcept
{
        auto to = (char*)dst;
        auto from = (const char*)src;

        std::ptrdiff_t sz = size;
        switch (sz % 16)
        {
        case 0: do {*to++ = *from++;
        case 15: *to++ = *from++;
        case 14: *to++ = *from++;
        case 13: *to++ = *from++;
        case 12: *to++ = *from++;
        case 11: *to++ = *from++;
        case 10: *to++ = *from++;
        case 9: *to++ = *from++;
        case 8: *to++ = *from++;
        case 7: *to++ = *from++;
        case 6: *to++ = *from++;
        case 5: *to++ = *from++;
        case 4: *to++ = *from++;
        case 3: *to++ = *from++;
        case 2: *to++ = *from++;
        case 1: *to++ = *from++;
        } while ((sz -= 16) > 0);
        }
}
*/

inline void mem_copy(void* dst, const void* src, std::size_t size) noexcept
{
        std::memcpy(dst, src, size);
}
