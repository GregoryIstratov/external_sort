#ifndef EXTERNAL_SORT_FILE_IO_HPP
#define EXTERNAL_SORT_FILE_IO_HPP

#include <fstream>
#include <vector>
#include <cstring>

void file_write(const char* filename, const void* data, size_t size);

class raw_file_reader
{
public:
        explicit
        raw_file_reader(const std::string& filename)
                : filename_(filename)
        {
                is_.open(filename_, std::ios::in | std::ios::binary);

                if(!is_)
                        throw std::runtime_error("Cannot open the file '"
                                                 + filename
                                                 + "': "
                                                 + std::string(strerror(errno)));

                is_.rdbuf()->pubsetbuf(nullptr, 0);

                is_.seekg(0, std::ios::end);
                file_size_ = is_.tellg();
                is_.seekg(0, std::ios::beg);
        }

        raw_file_reader(const raw_file_reader&) = delete;
        raw_file_reader& operator=(const raw_file_reader&) = delete;


        raw_file_reader(raw_file_reader&& o) = default;
        raw_file_reader& operator=(raw_file_reader&& o) = default;

        void close()
        {
                if(is_)
                        is_.close();

                is_ = std::ifstream();
        }


        std::streamsize read(char* buff, std::streamsize size)
        {
                is_.read(buff, size);

                if(is_.bad() || (is_.bad() && !is_.eof()))
                        throw std::runtime_error("Cannot read the file '"
                                                 + filename_
                                                 + "': "
                                                 + std::string(strerror(errno)));

                std::streamsize r = is_.gcount();
                read_ += r;

                return r;
        }

        bool eof() const { return is_.eof() || read_ >= file_size_; }

        std::string filename() const { return filename_; }
        uint64_t file_size() const { return file_size_; }

private:
        std::ifstream is_;
        std::string filename_;
        uint64_t file_size_;
        uint64_t read_ = 0;
};

template<typename T>
class chunk_istream;

template<typename T>
class chunk_ostream
{
public:
        chunk_ostream() = default;

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
                        throw std::runtime_error("Can't open the file " + filename_);
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
        std::ofstream os_;
        std::string filename_;
        size_t buff_size_ = 0;
        std::vector<char> buff_;
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
        chunk_istream(std::string&& filename)
                : filename_(std::move(filename))
        {}

        chunk_istream(chunk_istream&& o) = default;
        chunk_istream& operator=(chunk_istream&& o) = default;

        void open(size_t buff_size)
        {
                buff_size_ = buff_size;
                buff_elem_n_ = get_buff_elem_n(buff_size);
                buffer_.resize(buff_elem_n_);

                is_.open(filename_, std::ios::in | std::ios::binary);

                if(!is_)
                        throw std::runtime_error("Cannot open the file '"
                                                 + filename_
                                                 + "': "
                                                 + std::string(strerror(errno)));

                is_.seekg(0, std::ios::end);
                file_size_ = is_.tellg();
                is_.seekg(0, std::ios::beg);

                if (file_size_ % elem_size)
                        throw std::runtime_error("File '" + filename_
                                                 + "' is broken, the size must be a product of "
                                                 + std::to_string(elem_size));

                is_.rdbuf()->pubsetbuf((char*)&buffer_[0], buff_size);

                if (!next())
                        throw std::runtime_error("Can't read the file " + filename_
                                                 + " seems like it's empty");
        }

        const T& value() const { return val_; }

        bool next()
        {
                if (read_ >= file_size_)
                        return false;

                is_.read((char*)&val_, elem_size);

                if(is_.bad() || (is_.bad() && !is_.eof()))
                        throw std::runtime_error("Cannot read the file '"
                                                 + filename_
                                                 + "': "
                                                 + std::string(strerror(errno)));

                read_ += is_.gcount();

                return is_.gcount() == elem_size;
        }

        bool eof() const { return is_.eof() || read_ >= file_size_; }

        void copy(chunk_ostream<T>& os)
        {
                os.put(value());

                os.os_ << is_.rdbuf();
        }

        std::string filename() const { return filename_; }

        uint64_t size() const { return file_size_; }
        uint64_t count() const { return file_size_ / elem_size; }

        size_t buff_size() const { return buff_size_; }

private:
        size_t get_buff_elem_n(size_t buff_size)
        {
                if (buff_size % elem_size)
                        throw std::runtime_error(
                                std::string("chunk_istream: buff_size must be a product of ")
                                + std::to_string(elem_size));

                return buff_size / elem_size;
        }
private:
        std::string filename_;
        size_t buff_size_;
        size_t buff_elem_n_;
        std::ifstream is_;
        std::vector<uint32_t> buffer_;
        T val_;
        uint64_t file_size_;
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

#endif //EXTERNAL_SORT_FILE_IO_HPP
