#pragma once
#include <cstddef>

template<typename T>
class span
{
public:
        span(T* const ptr, std::size_t size)
                : ptr_(ptr), size_(size)
        {}

        T* begin() const { return ptr_; }
        T* end() const { return begin() + size_; }
private:
        T* const ptr_;
        std::size_t size_;
};
