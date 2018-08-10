#pragma once
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <vector>
#include <random>
#include <functional>
#include <algorithm>

#include "exception.hpp"
#include "util.hpp"
#include "literals.hpp"
#include <fstream>
#include "../config.hpp"

void delete_file(const char* filename);

void iterate_dir(const char* path, std::function<void(const char*)>&& callback);

bool check_dir_exist(const char* path);

void create_directory(const char* name);

void _file_write(std::string&& filename, const void* data, size_t size);

template<typename String>
void file_write(String&& filename, const void* data, size_t size)
{
        _file_write(std::forward<String>(filename), data, size);
}

std::vector<unsigned char> _file_read_all(std::string&& filename);

template<typename String>
std::vector<unsigned char> file_read_all(String filename)
{
        return _file_read_all(std::forward<String>(filename));
}
template<typename T>
void gen_rnd_test_file(const char* filename, uint64_t size)
{
        static_assert(std::is_integral<T>::value, "T must be integral");

        if (size % sizeof(T))
                THROW_EXCEPTION("Size must be product of " << sizeof(T));

        uint64_t N = size / sizeof(T);

        FILE *f = fopen(filename, "wb");

        if (!f)
                THROW_EXCEPTION("Can't create the file '"
                        << filename << "': " << strerror(errno));

        struct file_closer
        {
                explicit
                        file_closer(FILE* _f) : f_(_f) {}
                ~file_closer()
                {
                        if (f_) fclose(f_);
                }

                FILE* f_;
        } closer(f);

        std::random_device rd;
        std::default_random_engine e(rd());

        std::uniform_int_distribution<T> dis;

        constexpr size_t buff_size = 1_MiB;
        char buff[buff_size]{};

        setvbuf(f, buff, _IOFBF, buff_size);

        for (uint64_t i = 0; i < N; ++i) {
                T v = dis(e);
                fwrite(&v, sizeof(T), 1, f);
        }
}

template<typename T>
void make_rnd_file_from(std::vector<T>& arr, const char* filename)
{
        std::random_device rd;
        std::mt19937 g(rd());

        std::shuffle(arr.begin(), arr.end(), g);

        file_write(filename, arr.data(), arr.size() * sizeof(T));
}

class raw_file_buffer
{
protected:
        raw_file_buffer()
                : buffer_(PAGE_SIZE)
        {                
        }

        void set_buffer_to(std::ios& ios)
        {
                ios.rdbuf()->pubsetbuf(&buffer_[0], buffer_.size());
        }

        std::vector<char> buffer_;
};

class raw_file_reader : protected raw_file_buffer
{
public:
        explicit
                raw_file_reader(std::string filename)
                : filename_(std::move(filename))
        {
                is_.open(filename_, std::ios::in | std::ios::binary);

                if (!is_)
                        THROW_EXCEPTION("Cannot open the file '"
                                << filename_
                                << "': "
                                << strerror(errno));

                set_buffer_to(is_);
                
                is_.seekg(0, std::ios::end);
                file_size_ = is_.tellg();
                is_.seekg(0, std::ios::beg);
        }

        ~raw_file_reader()
        {
                close();
        }


        raw_file_reader(raw_file_reader&& o) noexcept
                : is_(std::move(o.is_)),
                filename_(std::move(o.filename_)),
                file_size_(zero_move(o.file_size_)),
                read_(zero_move(o.read_))
        {
        }

        raw_file_reader& operator=(raw_file_reader&& o) = delete;

        void close()
        {
                is_ = decltype(is_)();
        }


        std::streamsize read(char* buff, std::streamsize size)
        {
                is_.read(buff, size);

                if (!is_.eof() && is_.bad())
                        THROW_EXCEPTION("Cannot read the file '"
                                << filename_
                                << "': "
                                << strerror(errno));

                auto r = is_.gcount();
                read_ += r;

                return r;
        }

        bool is_open() const { return is_.is_open(); }

        bool eof() const { return is_.eof() || read_ >= file_size_; }

        std::string filename() const { return filename_; }
        uint64_t file_size() const { return file_size_; }

private:
        std::ifstream is_;
        std::string filename_;
        uint64_t file_size_ = 0;
        uint64_t read_ = 0;
};

class raw_file_writer : protected raw_file_buffer
{
public:
        explicit
                raw_file_writer(std::string filename)
                : filename_(std::move(filename))
        {
                set_buffer_to(os_);

                os_.open(filename_, std::ios::out | std::ios::trunc 
                                    | std::ios::binary);

                if (!os_)
                        THROW_EXCEPTION("Cannot open the file '"
                                << filename_
                                << "': "
                                << strerror(errno));
        }

        ~raw_file_writer()
        {
                close();
        }

        raw_file_writer(raw_file_writer&& o) noexcept
                : os_(std::move(o.os_)),
                filename_(std::move(o.filename_))
        {
        }

        raw_file_writer& operator=(raw_file_writer&& o) = delete;

        void close()
        {
                os_ = decltype(os_)();       
        }

        void write(const void* buff, std::size_t size)
        {
                os_.write((char*)buff, size);

                if (os_.bad())
                        THROW_EXCEPTION("Cannot write the file '"
                                << filename_
                                << "': "
                                << strerror(errno));
        }

        std::string filename() const { return filename_; }
private:
        std::ofstream os_;
        std::string filename_;
};

struct memory_mapped_file_source
{
        virtual ~memory_mapped_file_source() = default;

        virtual void open(const char* filename) = 0;
        virtual void close() = 0;
        virtual size_t size() const = 0;
        virtual const char* data() const = 0;

        static std::unique_ptr<memory_mapped_file_source> create();
};

struct memory_mapped_file_sink
{
        virtual ~memory_mapped_file_sink() = default;

        virtual void open(const char* filename, size_t size, 
                          size_t offset, bool create) = 0;
        virtual void close() = 0;
        virtual char* data() const = 0;

        static std::unique_ptr<memory_mapped_file_sink> create();
};
