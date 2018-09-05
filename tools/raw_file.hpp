#pragma once
#include <fstream>
#include <cstring>
#include <vector>
#include "exception.hpp"
#include "util.hpp"
#include "../config.hpp"

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
                        THROW_FILE_EXCEPTION(filename_) << "Cannot open the file";

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
                        THROW_FILE_EXCEPTION(filename_) << "Cannot read the file";

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
                        THROW_FILE_EXCEPTION(filename_) << "Cannot open the file";
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
                        THROW_FILE_EXCEPTION(filename_) << "Cannot write the file";
        }

        std::string filename() const { return filename_; }
private:
        std::ofstream os_;
        std::string filename_;
};
