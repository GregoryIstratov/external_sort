#pragma once
#include <type_traits>
#include <thread>
#include <iomanip>
#include <sstream>

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
