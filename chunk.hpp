#pragma once

#include <fstream>
#include <vector>
#include <list>
#include <cstring>
#include <mutex>
#include <map>
#include <memory>
#include <sstream>

#include "settings.hpp"
#include "log.hpp"
#include "util.hpp"

struct chunk_id
{
        using id_t = uint32_t;
        using lvl_t = uint32_t;

        chunk_id() : _bits((uint32_t)-1) {}
        ~chunk_id() = default;

        chunk_id(lvl_t _lvl, id_t _id)
                : lvl(_lvl), id(_id)
        {}


        chunk_id(const chunk_id&) noexcept = default;
        chunk_id& operator=(const chunk_id&) noexcept = default;

        chunk_id(chunk_id&& o) noexcept
                : _bits(o._bits)
        {
                o._bits = (uint64_t)-1;
        }

        chunk_id& operator=(chunk_id&& o) noexcept
        {
                if (&o == this)
                        return *this;

                _bits = o._bits;
                o._bits = (uint64_t)-1;

                return *this;
        }

        union
        {
                struct
                {
                        lvl_t lvl;
                        id_t  id;
                };

                uint64_t _bits;
        };
};


inline
bool operator<(const chunk_id& a, const chunk_id& b)
{
        return a._bits < b._bits;
}

inline
bool operator==(const chunk_id& a, const chunk_id& b)
{
        return a._bits == b._bits;
}

inline
std::string make_filename(uint32_t lvl, uint32_t id)
{
        std::stringstream ss;
        ss << CONFIG_SCHUNK_FILENAME_PAT
           << lvl << "_" << id;

        return ss.str();
}

inline
std::string make_filename(chunk_id id)
{
        return make_filename(id.lvl, id.id);
}

inline
std::ostream& operator<<(std::ostream& os, chunk_id id)
{
        os << make_filename(id);
        return os;
}

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
                fwrite((char*)&v, sizeof(T), 1,  os_);
        }

        void close() noexcept
        {
                if(os_)
                {
                        fclose(os_);
                        os_ = nullptr;
                }

                buff_ = std::vector<char>();
        }

        size_t buff_size() const { return buff_size_; }

        void filename(const std::string& value) { filename_ = value; }
        std::string filename() { return filename_; }
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

template<typename T>
class chunk_istream_iterator;

template<typename T>
class chunk_istream
{
public:
        static constexpr size_t elem_size = sizeof(T);

        explicit
        chunk_istream(chunk_id id)
                : id_(id),
                  filename_(make_filename(id))
        {}


        explicit
        chunk_istream(std::string&& filename)
                : id_(),
                  filename_(std::move(filename))
        {}

        ~chunk_istream()
        {
                release();
        }

        chunk_istream(chunk_istream&& o) = default;
        chunk_istream& operator=(chunk_istream&& o) = default;

        void open(size_t buff_size)
        {
                read_ = 0;
                buff_size_ = buff_size;
                buff_elem_n_ = get_buff_elem_n(buff_size);
                buffer_.resize(buff_elem_n_);

                is_ = fopen(filename_.c_str(), "rb");

                if(!is_)
                        throw_exception("Cannot open the file '"
                                                 << filename_
                                                 << "': "
                                                 << strerror(errno));

                fseek(is_, 0, SEEK_END);
                file_size_ = ftell(is_);
                rewind(is_);

                if (file_size_ % elem_size)
                        throw_exception("File '" << filename_
                                                 << "' is broken, the size must be a product of "
                                                 << elem_size);

                setvbuf(is_, (char*)&buffer_[0], _IOFBF, buff_size);
                
                if (!next())
                        throw_exception("Can't read the file " << filename_
                                                 << " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                size_t r = fread((char*)&val_, 1, elem_size, is_);

                if(ferror(is_))
                        throw_exception("Cannot read the file '"
                                                 << filename_
                                                 << "': "
                                                 << strerror(errno));

                read_ += r;

                return r == elem_size;
        }
        bool eof() const { return feof(is_) || read_ >= file_size_; }

        void release() noexcept
        {
                if(is_)
                {
                        fclose(is_);
                        is_ = nullptr;
                }

                buffer_ = std::vector<T>();
        }

        void copy_to(chunk_ostream<T>& os)
        {
                os.put(value());

                char buff[4 * KILOBYTE];

                while(!feof(is_))
                {
                        size_t r = fread(buff, 1, 4 * KILOBYTE, is_);

                        fwrite(buff, 1, r, os.os_);
                }
        }

        chunk_id id() const { return id_; }
        const std::string& filename() const { return filename_; }

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
        std::string filename_;
        size_t buff_size_;
        size_t buff_elem_n_;
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
        friend class chunk_istream<T>;
public:
        explicit
        chunk_istream_iterator(chunk_istream<T>& p)
        {
                if (p.eof())
                        p_ = (chunk_istream<T>*)~0;
                else
                        p_ = &p;
        }
        chunk_istream_iterator()
                : p_((chunk_istream<T>*)~0)
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
                        p_ = (chunk_istream<T>*)~0;
                }

                return *this;
        }
private:
        chunk_istream<T>* p_;
};
