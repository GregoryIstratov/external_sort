#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include "literals.hpp"

class fmt_guard
{
public:
        explicit fmt_guard(std::ios& _ios) : ios_(_ios), init_(nullptr)
        {
                init_.copyfmt(ios_);
        }

        ~fmt_guard()
        {
                ios_.copyfmt(init_);
        }

private:
        std::ios& ios_;
        std::ios init_;
};

inline
std::string size_format(uint64_t bytes)
{
        std::stringstream ss;

        if (bytes < 1024)
                ss << bytes << "Bytes";
        else if (bytes < 1_MiB)
                ss << (bytes / 1_KiB) << "KiB";
        else if (bytes < 1_GiB)
                ss << (bytes / 1_MiB) << "MiB";
        else
                ss << (bytes / 1_GiB) << "GiB";

        return ss.str();
}

inline
std::string num_format(uint64_t n)
{
        std::stringstream ss;

        if (n < 1000)
                ss << n;
        else if (n < 1000 * 1000)
                ss << (n / 1000) << "K";
        else
                ss << (n / 1000 / 1000) << "M";

        return ss.str();
}

namespace format
{

        struct _lower_case {};
        static constexpr _lower_case lower_case;

        struct _upper_case {};
        static constexpr _upper_case upper_case;

        template<typename Case>
        struct _hex_format_parameter;

        template<>
        struct _hex_format_parameter<_upper_case>
        {
                static constexpr const char* value = "%X";
        };

        template<>
        struct _hex_format_parameter<_lower_case>
        {
                static constexpr const char* value = "%x";
        };

        template<typename T, typename Case>
        std::string to_hex_string(T num, Case)
        {
                static_assert(std::is_integral<T>::value,
                              "T must be of an integral type");

                // each byte can be represented as a 2 digit number in hex
                constexpr size_t hex_digits = sizeof(T) * 2
                        + (std::numeric_limits<T>::is_signed ? 1 : 0);

                char buff[hex_digits + 1];

                std::snprintf(buff, hex_digits,
                              _hex_format_parameter<Case>::value, num);

                return std::string(buff);
        }

        template <typename T>
        std::string to_hex_string(T num)
        {
                return to_hex_string(num, lower_case);
        }

}
