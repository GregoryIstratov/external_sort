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

void gen_rnd_test_file(const char* filename, uint64_t size);

template<typename T>
void make_rnd_file_from(std::vector<T>& arr, const char* filename)
{
        std::random_device rd;
        std::mt19937 g(rd());

        std::shuffle(arr.begin(), arr.end(), g);

        file_write(filename, arr.data(), arr.size() * sizeof(T));
}

class raw_file_reader
{
public:
        explicit
                raw_file_reader(std::string filename)
                : filename_(std::move(filename))
        {
                is_ = fopen(filename_.c_str(), "rb");

                if (!is_)
                        throw_exception("Cannot open the file '"
                                << filename_
                                << "': "
                                << strerror(errno));

                setvbuf(is_, nullptr, _IONBF, 0);

                fseek(is_, 0, SEEK_END);
                file_size_ = ftell(is_);
                rewind(is_);
        }

        ~raw_file_reader()
        {
                close();
        }


        raw_file_reader(raw_file_reader&& o) noexcept
                : is_(zero_move(o.is_)),
                filename_(std::move(o.filename_)),
                file_size_(zero_move(o.file_size_)),
                read_(zero_move(o.read_))
        {
        }

        raw_file_reader& operator=(raw_file_reader&& o) = delete;

        void close()
        {
                if (is_) {
                        fclose(is_);
                        is_ = nullptr;
                }
        }


        std::streamsize read(char* buff, std::streamsize size)
        {
                auto r = fread(buff, 1, size, is_);

                if (ferror(is_))
                        throw_exception("Cannot read the file '"
                                << filename_
                                << "': "
                                << strerror(errno));
                read_ += r;

                return r;
        }

        bool is_opened() const { return is_ != nullptr; }

        bool eof() const { return feof(is_) || read_ >= file_size_; }

        std::string filename() const { return filename_; }
        uint64_t file_size() const { return file_size_; }

private:
        FILE * is_ = nullptr;
        std::string filename_;
        uint64_t file_size_ = 0;
        uint64_t read_ = 0;
};

class raw_file_writer
{
public:
        explicit
                raw_file_writer(std::string filename)
                : filename_(std::move(filename))
        {
                is_ = fopen(filename_.c_str(), "wb");

                if (!is_)
                        throw_exception("Cannot open the file '"
                                << filename_
                                << "': "
                                << strerror(errno));

                setvbuf(is_, nullptr, _IONBF, 0);
        }

        ~raw_file_writer()
        {
                close();
        }

        raw_file_writer(raw_file_writer&& o) noexcept
                : is_(zero_move(o.is_)),
                filename_(std::move(o.filename_))
        {
        }

        raw_file_writer& operator=(raw_file_writer&& o) = delete;

        void close()
        {
                if (is_) {
                        fclose(is_);
                        is_ = nullptr;
                }
        }

        void write(const void* buff, std::size_t size)
        {
                fwrite(buff, 1, size, is_);

                if (ferror(is_))
                        throw_exception("Cannot write the file '"
                                << filename_
                                << "': "
                                << strerror(errno));
        }

        std::string filename() const { return filename_; }
private:
        FILE * is_ = nullptr;
        std::string filename_;
};
