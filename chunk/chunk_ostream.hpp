#pragma once

#include <vector>
#include "../tools/exception.hpp"
#include "chunk_stream.hpp"

template<typename T>
class _chunk_ostream<T, chunk_stream_cpp>
{
public:
        _chunk_ostream() = default;

        explicit _chunk_ostream(std::string&& filename)
                : filename_(std::move(filename))

        {}

        ~_chunk_ostream()
        {
                close();
        }

        _chunk_ostream(_chunk_ostream&&) = default;
        _chunk_ostream& operator=(_chunk_ostream&&) = default;

        void open(size_t buff_size, uint64_t)
        {
                buff_size_ = buff_size;
                buff_.resize(buff_size);

                os_.open(filename_, std::ios::out | std::ios::trunc
                        | std::ios::binary);

                if (!os_)
                        THROW_EXCEPTION("Can't open file '" << filename_
                                << "': " << strerror(errno));

                os_.rdbuf()->pubsetbuf(&buff_[0], buff_size);
        }

        void put(T v)
        {
                os_.write((char*)&v, sizeof(T));
        }

        void close() noexcept
        {
                os_ = decltype(os_)();
                buff_ = std::vector<char>();
        }

        size_t buff_size() const { return buff_size_; }

        void filename(const std::string& value) { filename_ = value; }
        std::string filename() const { return filename_; }
private:
        friend class _chunk_istream<T, chunk_stream_cpp>;

private:
        std::vector<char> buff_;
        std::ofstream os_;
        std::string filename_;
        size_t buff_size_ = 0;
};

template<typename T>
class _chunk_ostream<T, chunk_stream_stdio>
{
public:
        _chunk_ostream() = default;

        explicit _chunk_ostream(std::string&& filename)
                        : filename_(std::move(filename))

        {}

        ~_chunk_ostream()
        {
                close();
        }

        _chunk_ostream(_chunk_ostream&&) = default;
        _chunk_ostream& operator=(_chunk_ostream&&) = default;

        void open(size_t buff_size, uint64_t)
        {
                buff_size_ = buff_size;
                buff_.resize(buff_size);


                os_ = fopen(filename_.c_str(), "wb");
                if (!os_)
                        THROW_EXCEPTION("Can't open file '" << filename_
                                        << "': " << strerror(errno));

                setvbuf(os_, &buff_[0], _IOFBF, buff_size);
        }

        void put(T v)
        {
                fwrite((char*)&v, sizeof(T), 1, os_);
        }

        void close() noexcept
        {
                if (os_)
                {
                        fclose(os_);
                        os_ = nullptr;
                }

                buff_ = std::vector<char>();
        }

        size_t buff_size() const { return buff_size_; }

        void filename(const std::string& value) { filename_ = value; }
        std::string filename() const { return filename_; }
private:
        friend class _chunk_istream<T, chunk_stream_stdio>;

private:
        std::vector<char> buff_;
        FILE* os_ = nullptr;
        std::string filename_;
        size_t buff_size_ = 0;
};

template<typename T>
class _chunk_ostream<T, chunk_stream_mmap>
{
public:
        _chunk_ostream() = default;

        explicit _chunk_ostream(std::string&& filename)
                : filename_(std::move(filename))

        {}

        ~_chunk_ostream()
        {
                close();
        }

        _chunk_ostream(_chunk_ostream&&) = default;
        _chunk_ostream& operator=(_chunk_ostream&&) = default;

        void open(size_t buff_size, uint64_t output_size)
        {
                buff_size_ = buff_size;
                //buff_.resize(buff_size);

                sink_->open(filename_.c_str(), output_size, 0, true);

                data_ = reinterpret_cast<T*>(sink_->data());
        }

        void put(T v)
        {
                *data_++ = v;
        }

        void close() noexcept
        {
                sink_.reset();
                buff_ = std::vector<char>();
        }

        size_t buff_size() const { return buff_size_; }

        void filename(const std::string& value) { filename_ = value; }
        std::string filename() const { return filename_; }
private:
        friend class _chunk_istream<T, chunk_stream_stdio>;

private:
        using file_sink = memory_mapped_file_sink;
        using file_sink_ptr = std::unique_ptr<file_sink>;

        std::vector<char> buff_;
        file_sink_ptr sink_ = file_sink::create();
        std::string filename_;
        size_t buff_size_ = 0;
        T* data_ = nullptr;
};

template<typename T>
class chunk_ostream_iterator
        : public std::iterator<std::output_iterator_tag, void, void, void, void>
{
public:
        explicit
                chunk_ostream_iterator(chunk_ostream<T>& os)
                : os_(&os)
        {
        }
        chunk_ostream_iterator(const chunk_ostream_iterator& it)
                : os_(it.os_)
        {}

        chunk_ostream_iterator& operator=(const T& v)
        {
                os_->put(v);

                return *this;
        }

        chunk_ostream_iterator& operator*()
        {
                return *this;
        }

        chunk_ostream_iterator& operator++()
        {
                return *this;
        }

        chunk_ostream_iterator& operator++(int)
        {
                return *this;
        }
private:
        chunk_ostream<T>* os_;
};
