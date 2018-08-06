#pragma once

#include <vector>
#include "../tools/exception.hpp"


template<typename T>
class chunk_istream;

template<typename T>
class chunk_ostream
{
public:
        chunk_ostream() = default;

        explicit
                chunk_ostream(std::string&& filename)
                : filename_(std::move(filename))

        {}

        ~chunk_ostream()
        {
                close();
        }

        chunk_ostream(chunk_ostream&&) = default;
        chunk_ostream& operator=(chunk_ostream&&) = default;

        void open(size_t buff_size)
        {
                buff_size_ = buff_size;
                buff_.resize(buff_size);


                os_ = fopen(filename_.c_str(), "wb");
                if (!os_)
                        throw_exception("Can't open the file " << filename_);

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
        friend class chunk_istream<T>;

private:
        std::vector<char> buff_;
        FILE* os_ = nullptr;
        std::string filename_;
        size_t buff_size_ = 0;
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
