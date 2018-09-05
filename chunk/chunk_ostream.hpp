#pragma once

#include <vector>
#include "../tools/exception.hpp"
#include "../tools/mapped_file.hpp"
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
                        THROW_FILE_EXCEPTION(filename_) << "Cannot open the file";

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
                        THROW_FILE_EXCEPTION(filename_) << "Cannot open the file";

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
                width_n_ = buff_size / sizeof(T);
                size_n_  = output_size / sizeof(T);

                file_->open(filename_.c_str(), size_n_ * sizeof(T),
                            std::ios::out | std::ios::trunc);

                if (!load_next_window())
                        THROW_EXCEPTION << "load_next_window failed " << filename_;
        }

        void put(T v)
        {
                if (cur_ >= width_n_)
                {
                        load_next_window();
                }

                data_[cur_++] = v;
        }

        void close() noexcept
        {
                range_.reset();
                file_.reset();
        }

        size_t buff_size() const { return width_n_ * sizeof(T); }

        void filename(const std::string& value) { filename_ = value; }
        std::string filename() const { return filename_; }
private:
        friend class _chunk_istream<T, chunk_stream_stdio>;

        //TODO move it to a base class with chunk_istream
        bool load_next_window()
        {
                width_n_ = std::min(width_n_, size_n_ - offset_);

                if (width_n_ == 0)
                {
                        if(IS_ENABLED(CONFIG_DEBUG))
                        {
                                if (!range_)
                                        THROW_EXCEPTION << "Trying to write after EOF";
                        }

                        range_.reset();
                        return false;
                }

                range_ = file_->range(offset_ * sizeof(T), width_n_ * sizeof(T));
                range_->lock();
                data_ = reinterpret_cast<T*>(range_->data());

                offset_ += width_n_;
                cur_ = 0;

                return true;
        }
private:
        chunk_id id_;

        mapped_file_uptr file_ = mapped_file::create();
        mapped_range_uptr range_;

        T* data_ = nullptr;
        std::size_t size_n_ = 0, width_n_ = 0;
        std::size_t offset_ = 0, cur_ = 0;

        std::string filename_;
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
