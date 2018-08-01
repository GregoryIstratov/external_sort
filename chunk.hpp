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


        chunk_ostream(chunk_ostream&&) = default;
        chunk_ostream& operator=(chunk_ostream&&) = default;

        void open(size_t buff_size)
        {
                buff_size_ = buff_size;
                buff_.resize(buff_size);

                os_.rdbuf()->pubsetbuf(&buff_[0], buff_size);
                os_.open(filename_, std::ios::out | std::ios::trunc | std::ios::binary);

                if (!os_)
                        throw_exception("Can't open the file " << filename_);
        }

        void put(T v)
        {
                os_.write((char*)&v, sizeof(T));
        }

        void close()
        {
                os_.flush();
                os_.close();

                os_ = std::ofstream();
                buff_ = std::vector<char>();
        }

        size_t buff_size() const { return buff_size_; }

        void filename(const std::string& value) { filename_ = value; }
        std::string filename() { return filename_; }
private:
        friend class chunk_istream<T>;

private:
        std::vector<char> buff_;
        std::ofstream os_;
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


        chunk_istream(chunk_istream&& o) = default;
        chunk_istream& operator=(chunk_istream&& o) = default;

        void open(size_t buff_size)
        {
                read_ = 0;
                buff_size_ = buff_size;
                buff_elem_n_ = get_buff_elem_n(buff_size);
                buffer_.resize(buff_elem_n_);

                is_.open(filename_, std::ios::in | std::ios::binary);

                if(!is_)
                        throw_exception("Cannot open the file '"
                                                 << filename_
                                                 << "': "
                                                 << strerror(errno));

                is_.seekg(0, std::ios::end);
                file_size_ = is_.tellg();
                is_.seekg(0, std::ios::beg);

                if (file_size_ % elem_size)
                        throw_exception("File '" << filename_
                                                 << "' is broken, the size must be a product of "
                                                 << elem_size);

                is_.rdbuf()->pubsetbuf((char*)&buffer_[0], buff_size);
                
                if (!next())
                        throw_exception("Can't read the file " << filename_
                                                 << " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                is_.read((char*)&val_, elem_size);

                if(is_.bad() || (is_.bad() && !is_.eof()))
                        throw_exception("Cannot read the file '"
                                                 << filename_
                                                 << "': "
                                                 << strerror(errno));

                read_ += is_.gcount();

                return is_.gcount() == elem_size;
        }

        void debug_dump()
        {
                pos_guard pg(is_);
                is_.seekg(0, std::ios::beg);

                std::stringstream ss;

                ss << "chunk(" << make_filename(id()) << "): [";

                chunk_istream_iterator<T> beg(*this), end;

                std::copy(beg, end, std::ostream_iterator<T>(ss, " "));

                ss << "]";

                debug() << ss.rdbuf();
        }

        int64_t search(T value)
        {
                std::ifstream is(filename(), std::ios::in | std::ios::binary);

                if(!is)
                        throw_exception("Cannot open the file for search'"
                                                 << filename_
                                                 << "': "
                                                 << strerror(errno));


                //is.rdbuf()->pubsetbuf(nullptr, 0);

                pos_guard pg(is_);

                is.seekg(0, std::ios::beg);

                int64_t l,r, m;

                l = 0;
                r = count() - 1;

                while(l <= r)
                {
                        m = (l + r) / 2;

                        is.seekg(get_el_offset(m));

                        T a;
                        is.read((char*) &a, elem_size);

                        if(value < a)
                                r = m - 1;
                        else if(value > a)
                                l = m + 1;
                        else
                                return m;
                }

                m = (l + r) / 2;
                return m;
        }

        bool eof() const { return is_.eof() || read_ >= file_size_; }

        void release()
        {
                is_ = std::ifstream();
                buffer_ = std::vector<T>();
        }

        void copy(chunk_ostream<T>& os)
        {
                os.put(value());

                os.os_ << is_.rdbuf();
        }

        chunk_id id() const { return id_; }
        const std::string& filename() const { return filename_; }

        uint64_t size() const { return file_size_; }
        uint64_t count() const { return file_size_ / elem_size; }

        size_t buff_size() const { return buff_size_; }

private:
        class pos_guard {
                std::ifstream& is_;
                decltype(is_.tellg()) pos_;
        public:
                explicit
                pos_guard(std::ifstream& is) : is_(is) {
                        pos_ = is_.tellg();
                }

                ~pos_guard()
                {
                        is_.seekg(pos_);
                }
        };

        uint64_t get_el_offset(uint64_t el)
        {
                return el * elem_size;
        }

        size_t get_buff_elem_n(size_t buff_size)
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
        std::ifstream is_;
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
