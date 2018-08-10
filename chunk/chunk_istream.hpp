#pragma once
#include "chunk_id.hpp"
#include "chunk_stream.hpp"
#include "../tools/exception.hpp"

template<typename T>
class _chunk_istream<T, chunk_stream_cpp>
{
public:
        static constexpr size_t elem_size = sizeof(T);

        _chunk_istream() = default;

        explicit _chunk_istream(chunk_id id)
                : id_(std::move(id))
        {}

        ~_chunk_istream()
        {
                release();
        }

        _chunk_istream(_chunk_istream&& o) = default;
        _chunk_istream& operator=(_chunk_istream&& o) = default;

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

                is_.open(filename, std::ios::in | std::ios::binary);

                if (!is_)
                        THROW_EXCEPTION("Cannot open the file '"
                                << filename
                                << "': "
                                << strerror(errno));

                is_.rdbuf()->pubsetbuf((char*)&buffer_[0], buff_size);

                is_.seekg(0, std::ios::end);
                file_size_ = is_.tellg();
                is_.seekg(0, std::ios::beg);

                if (file_size_ % elem_size)
                        THROW_EXCEPTION("File '" << filename
                                << "' is broken, the size must be a product of "
                                << elem_size);

                if (!next())
                        THROW_EXCEPTION("Can't read the file " << filename
                                << " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                is_.read((char*)&val_, elem_size);

                if (!is_.eof() && is_.bad())
                        THROW_EXCEPTION("Cannot read the file '"
                                << id().to_full_filename()
                                << "': "
                                << strerror(errno));

                auto r = is_.gcount();
                read_ += r;

                return r == elem_size;
        }
        bool eof() const { return is_.eof() || read_ >= file_size_; }

        void release() noexcept
        {
                is_ = decltype(is_)();
                buffer_ = std::vector<T>();
        }

        void copy_to(chunk_ostream<T>& os)
        {
                os.put(value());

                os.os_ << is_.rdbuf();
        }

        chunk_id id() const { return id_; }

        uint64_t size() const { return file_size_; }
        uint64_t count() const { return file_size_ / elem_size; }

        size_t buff_size() const { return buff_size_; }

private:
        static size_t get_buff_elem_n(size_t buff_size)
        {
                if (buff_size % elem_size)
                        THROW_EXCEPTION("buff_size="
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
        std::ifstream is_;
        T val_ = T();
        uint64_t file_size_ = 0;
        uint64_t read_ = 0;
};

template<typename T>
class _chunk_istream<T, chunk_stream_stdio>
{
public:
        static constexpr size_t elem_size = sizeof(T);

        _chunk_istream() = default;

        explicit _chunk_istream(chunk_id id)
                        : id_(std::move(id))
        {}

        ~_chunk_istream()
        {
                release();
        }

        _chunk_istream(_chunk_istream&& o) = default;
        _chunk_istream& operator=(_chunk_istream&& o) = default;

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
                        THROW_EXCEPTION("Cannot open the file '"
                                << filename
                                << "': "
                                << strerror(errno));

                setvbuf(is_, (char*)&buffer_[0], _IOFBF, buff_size);

                fseek(is_, 0, SEEK_END);
                file_size_ = ftell(is_);
                rewind(is_);

                if (file_size_ % elem_size)
                        THROW_EXCEPTION("File '" << filename
                                << "' is broken, the size must be a product of "
                                << elem_size);

                if (!next())
                        THROW_EXCEPTION("Can't read the file " << filename
                                << " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                size_t r = fread((char*)&val_, 1, elem_size, is_);

                if (ferror(is_))
                        THROW_EXCEPTION("Cannot read the file '"
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
                        THROW_EXCEPTION("buff_size="
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
class _chunk_istream<T, chunk_stream_mmap>
{
public:
        static constexpr size_t elem_size = sizeof(T);

        _chunk_istream()
        {
                
        }

        explicit _chunk_istream(chunk_id id)
                : id_(std::move(id))
        {}

        ~_chunk_istream()
        {
                release();
        }

        _chunk_istream(_chunk_istream&& o) = default;
        _chunk_istream& operator=(_chunk_istream&& o) = default;

        void open(size_t buff_size)
        {
                open(id().to_full_filename(), buff_size);
        }

        void open(std::string&& filename, size_t buff_size)
        {
                read_ = 0;
                buff_size_ = buff_size;
                buff_elem_n_ = get_buff_elem_n(buff_size);
                //buffer_.resize(buff_elem_n_);

                source_->open(filename.c_str());

                file_size_ = source_->size();

                if (file_size_ % elem_size)
                        THROW_EXCEPTION("File '" << filename
                                << "' is broken, the size must be a product of "
                                << elem_size);

                val_ptr_ = reinterpret_cast<const T*>(source_->data());

                if (!next())
                        THROW_EXCEPTION("Can't read the file " << filename
                                << " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                val_ = *val_ptr_++;
                read_ += sizeof(T);

                return true;
        }
        bool eof() const { return read_ >= file_size_; }

        void release() noexcept
        {
                source_.reset();
                buffer_ = std::vector<T>();
        }

        void copy_to(chunk_ostream<T>& os)
        {
                do
                {
                        os.put(value());
                } 
                while (next());
        }

        chunk_id id() const { return id_; }

        uint64_t size() const { return file_size_; }
        uint64_t count() const { return file_size_ / elem_size; }

        size_t buff_size() const { return buff_size_; }

private:
        static size_t get_buff_elem_n(size_t buff_size)
        {
                if (buff_size % elem_size)
                        THROW_EXCEPTION("buff_size="
                                << buff_size
                                << " must be a product of "
                                << elem_size);

                return buff_size / elem_size;
        }
private:
        using file_source = memory_mapped_file_source;
        using file_source_ptr = std::unique_ptr<file_source>;

        chunk_id id_;
        size_t buff_size_ = 0;
        size_t buff_elem_n_ = 0;
        std::vector<T> buffer_;
        file_source_ptr source_ = file_source::create();
        T val_ = T();
        const T* val_ptr_ = nullptr;
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
        : public std::iterator<std::input_iterator_tag, T, 
                               ptrdiff_t, const T*, const T&>
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
        chunk_istream<T>* p_;
};
