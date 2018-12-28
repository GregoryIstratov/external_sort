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
                THROW_EXCEPTION << "Size must be product of " << sizeof(T);

        uint64_t N = size / sizeof(T);

        FILE *f = fopen(filename, "wb");

        if (!f)
                THROW_FILE_EXCEPTION(filename) << "Cannot create the file";

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

/*
enum class file_io
{
        map,
        raw,
        native
};

template<typename T>
class mapped_input_file;

template<typename T>
class raw_input_file;

template<typename T>
struct input_file
{
        virtual ~input_file() = default;

        virtual void open(const char* filename) = 0;
        virtual void close() = 0;
        virtual std::unique_ptr<abstract_data_device<T>>
                get_region(std::streamsize pos, std::streamsize n) = 0;

        static std::unique_ptr<input_file<T>> create(file_io type)
        {
                switch (type)
                {
                case file_io::map:
                        return std::make_unique<mapped_input_file<T>>();

                case file_io::raw:
                        return std::make_unique<raw_input_file<T>>();
                }
        }
};

template<typename T>
class mapped_input_file : public input_file<T>
{
public:
        mapped_input_file()
                : source_(memory_mapped_file_source::create())
        {
                
        }

        void open(const char* filename) override
        {
                source_->open(filename);
        }

        void close() override
        {
                source_->close();
        }

        std::unique_ptr<source_data_device<T>>
                get_region(std::streamsize pos, std::streamsize n) override
        {
                const T* data = reinterpret_cast<const T*>(source_->data() 
                                                           + pos * sizeof(T));

                return std::make_unique<mapped_source_data_device<T>>(data, n);
        }

private:
        std::unique_ptr<memory_mapped_file_source> source_;
};

template<typename T>
class raw_input_file : public input_file<T>
{
public:
        void open(const char* filename) override
        {
                source_ = std::make_unique<raw_file_reader>(filename);
        }

        void close() override
        {
                source_->close();
        }

        std::unique_ptr<source_data_device<T>>
                get_region(std::streamsize pos, std::streamsize n) override
        {
                std::vector<T> buff(n);

                if (!source_->is_open() || source_->eof())
                {
                        return {};
                }

                auto to_read = n * sizeof(T);
                uint64_t read = source_->read(reinterpret_cast<char*>(&buff[0]),
                                              to_read);

                if (read != to_read) 
                {
                        if (read == 0) 
                        {
                                source_->close();
                                return {};
                        }

                        if (source_->eof())
                                source_->close();

                        size_t c = read / sizeof(T);
                        buff.erase(buff.begin() + c, buff.end());
                }
                

                return std::make_unique<buffered_source_data_device<T>>(
                        std::move(buff)
                        );
        }

private:
        std::unique_ptr<raw_file_reader> source_;
};
*/
