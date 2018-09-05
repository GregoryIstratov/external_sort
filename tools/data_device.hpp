#pragma once
#include <vector>

/*
#include "raw_file.hpp"

template<typename T>
struct mapped_mem
{
        virtual ~mapped_mem() = default;

        virtual const T* begin() const = 0;
        virtual const T* end()   const = 0;

        virtual T* begin() = 0;
        virtual T* end() = 0;

        virtual void flush() = 0;

        virtual std::size_t size() const = 0;
};

template<typename T>
class mapped_file_buffer : public mapped_mem<T>
{
public:
        mapped_file_buffer(std::vector<T>&& buffer, 
                           std::string filename)
                : buffer_(std::move(buffer)), filename_(std::move(filename))
        {
                
        }

        mapped_file_buffer(mapped_file_buffer&&) = delete;
        mapped_file_buffer& operator=(mapped_file_buffer&&) = delete;

        const T* begin() const override
        {
                return &(*buffer_.begin());
        }

        const T* end() const override
        {
                return &(*buffer_.end());
        }

        T* begin() override
        {
                return &(*buffer_.begin());
        }

        T* end() override
        {
                return &(*buffer_.end());
        }

        void flush() override
        {
                raw_file_writer fw(filename_);
                fw.write(buffer_.data(), buffer_.size() * sizeof(T));
        }

        std::size_t size() const override
        {
                return buffer_.size();
        }

private:
        std::vector<T> buffer_;
        std::string filename_;
};

template<typename T>
class buffered_sink_data_device : public sink_data_device<T>
{
public:
        explicit
                buffered_sink_data_device(std::vector<T>&& buffer)
                : buffer_(std::move(buffer))
        {

        }

        buffered_sink_data_device(buffered_sink_data_device&&) = delete;
        buffered_sink_data_device& operator=(buffered_sink_data_device&&) = delete;

        T* begin() override
        {
                return &(*buffer_.begin());
        }

        T* end() override
        {
                return &(*buffer_.end());
        }

        std::size_t size() const override
        {
                return buffer_.size();
        }

private:
        std::vector<T> buffer_;
};

template<typename T>
class mapped_source_data_device : public source_data_device<T>
{
public:
        explicit mapped_source_data_device(const T* const data, std::size_t size)
                : data_(data), size_(size)
        {

        }

        mapped_source_data_device(mapped_source_data_device&&) = delete;
        mapped_source_data_device& operator=(mapped_source_data_device&&) = delete;

        const T* begin() const override
        {
                return data_;
        }

        const T* end() const override
        {
                return data_ + size_;
        }

        std::size_t size() const override
        {
                return size_;
        }

private:
        const T* const data_;
        std::size_t size_;
};

*/
