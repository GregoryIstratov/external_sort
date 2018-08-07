#pragma once

#include "chunk_id.hpp"
#include "chunk_ostream.hpp"
#include "../tools/exception.hpp"

template<typename T>
class chunk_istream_iterator;

template<typename T>
class chunk_istream
{
public:
        static constexpr size_t elem_size = sizeof(T);

        chunk_istream() = default;

        explicit
                chunk_istream(chunk_id id)
                : id_(std::move(id))
        {}

        ~chunk_istream()
        {
                release();
        }

        chunk_istream(chunk_istream&& o) = default;
        chunk_istream& operator=(chunk_istream&& o) = default;

        void open(size_t buff_size)
        {
                open(id().to_full_filename(), buff_size);
        }

        void open(std::string&& filename, size_t buff_size)
        {
                read_ = 0;
                buff_size_ = buff_size;
                buff_elem_n_ = get_buff_elem_n(buff_size);
                buffer_.resize(buff_elem_n_);

                is_ = fopen(filename.c_str(), "rb");

                if (!is_)
                        throw_exception("Cannot open the file '"
                                << filename
                                << "': "
                                << strerror(errno));

                setvbuf(is_, (char*)&buffer_[0], _IOFBF, buff_size);

                fseek(is_, 0, SEEK_END);
                file_size_ = ftell(is_);
                rewind(is_);

                if (file_size_ % elem_size)
                        throw_exception("File '" << filename
                                << "' is broken, the size must be a product of "
                                << elem_size);

                if (!next())
                        throw_exception("Can't read the file " << filename
                                << " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                size_t r = fread((char*)&val_, 1, elem_size, is_);

                if (ferror(is_))
                        throw_exception("Cannot read the file '"
                                << id().to_full_filename()
                                << "': "
                                << strerror(errno));

                read_ += r;

                return r == elem_size;
        }
        bool eof() const { return feof(is_) || read_ >= file_size_; }

        void release() noexcept
        {
                if (is_)
                {
                        fclose(is_);
                        is_ = nullptr;
                }

                buffer_ = std::vector<T>();
        }

        void copy_to(chunk_ostream<T>& os)
        {
                os.put(value());

                char buff[PAGE_SIZE];

                while (!feof(is_))
                {
                        size_t r = fread(buff, 1, PAGE_SIZE, is_);

                        fwrite(buff, 1, r, os.os_);
                }
        }

        chunk_id id() const { return id_; }

        uint64_t size() const { return file_size_; }
        uint64_t count() const { return file_size_ / elem_size; }

        size_t buff_size() const { return buff_size_; }

private:
        static size_t get_buff_elem_n(size_t buff_size)
        {
                if (buff_size % elem_size)
                        throw_exception("buff_size="
                                << buff_size
                                << " must be a product of "
                                << elem_size);

                return buff_size / elem_size;
        }
private:
        chunk_id id_;
        size_t buff_size_ = 0;
        size_t buff_elem_n_ = 0;
        std::vector<T> buffer_;
        FILE* is_ = nullptr;
        T val_ = T();
        uint64_t file_size_ = 0;
        uint64_t read_ = 0;
};

template<typename T>
bool operator>(const chunk_istream<T>& a, const chunk_istream<T>& b)
{
        return a.value() > b.value();
}

template<typename T>
class chunk_istream_iterator
        : public std::iterator<std::input_iterator_tag, T, ptrdiff_t, const T*, const T&>
{
public:
        
        explicit
                chunk_istream_iterator(chunk_istream<T>& p)
        {
                if (p.eof())
                        p_ = (chunk_istream<T>*)(uintptr_t) - 1;
                else
                        p_ = &p;
        }
        chunk_istream_iterator()
                : p_((chunk_istream<T>*)(uintptr_t) - 1)
        {
        }

        chunk_istream_iterator(const chunk_istream_iterator& it)
                : p_(it.p_)
        {}

        bool operator!=(chunk_istream_iterator const& other) const
        {
                return p_ != other.p_;
        }

        bool operator==(chunk_istream_iterator const& other) const
        {
                return p_ == other.p_;
        }

        typename chunk_istream_iterator::reference operator*() const
        {
                return p_->value();
        }

        chunk_istream_iterator& operator++()
        {
                if (!p_->next())
                {
                        p_ = (chunk_istream<T>*)(uintptr_t) - 1;
                }

                return *this;
        }
private:
        friend class chunk_istream<T>;

        chunk_istream<T>* p_;
};
