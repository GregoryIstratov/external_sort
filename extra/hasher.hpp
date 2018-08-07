#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include "crc64.hpp"

template<size_t HashSize>
class hash_value
{
public:
        hash_value() = default;

        template<typename T, typename = 
                typename std::enable_if<!std::is_pointer<
                        typename std::decay<T>::type>::value>::type>
        explicit 
        hash_value(const T& value)
        {
                set(value);
        }

        template<typename T>
        explicit 
        hash_value(const T* value)
        {
                set(value);
        }

        template<typename T, typename =
                typename std::enable_if<!std::is_pointer<
                        typename std::decay<T>::type>::value>::type>
        void set(const T& value)
        {
                static_assert(sizeof(T) == HashSize,
                        "Hash size and T size doesn't match");

                _set(&value);
        }

        template<typename T>
        void set(const T* value)
        {
                _set(value);
        }

        template<typename T>
        T cast() const { return *reinterpret_cast<const T*>(data_.data()); }

        auto cbegin() const { return data_.cbegin(); }
        auto cend() const { return data_.cend(); }

        constexpr auto size() const { return HashSize; }

        const auto* data() const { return data_.data(); }

        friend std::ostream& operator<<(std::ostream& os, const hash_value& hs)
        {
                fmt_guard guard(os);
                os << std::hex;

                for (int32_t i = HashSize - 1; i >= 0; --i)
                        os << std::setfill('0') << std::setw(2) 
                           << (uint32_t)hs.data_[i];

                return os;
        }

        friend bool operator==(const hash_value& a, const hash_value& b)
        {
                for (size_t i = 0; i < HashSize; ++i)
                        if (a.data_[i] != b.data_[i])
                                return false;

                return true;
        }

private:
        void _set(const void* value)
        {
                auto p = reinterpret_cast<const uint8_t*>(value);

                /* can be unrolled by compilator */
                for (size_t i = 0; i < HashSize; ++i)
                        data_[i] = p[i];
        }

private:
        std::array<uint8_t, HashSize> data_;
};

class hasher_crc64
{
public:
        template<typename T, typename = typename std::enable_if<std::is_trivial<T>::value>::type>
        void put(T value)
        {
                hash_ = crc64(
                        hash_,
                        reinterpret_cast<const uint8_t*>(&value),
                        sizeof(T)
                );
        }

        void put(const std::string& s)
        {
                hash_ = crc64(
                        hash_,
                        reinterpret_cast<const uint8_t*>(s.data()),
                        s.size()
                );
        }

        template<typename T>
        void put(const std::vector<T>& v)
        {
                hash_ = crc64(
                        hash_,
                        reinterpret_cast<const uint8_t*>(v.data()),
                        v.size() * sizeof(T)
                );
        }

        auto hash() const { return hash_value<8>(hash_); }
private:
        uint64_t hash_ = 0;
};
