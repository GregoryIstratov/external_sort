#pragma once

#include <cstddef> // std::size_t
#include <memory> // std::unique_ptr
#include <ios> // std::ios::openmode

struct mapped_file;

enum class madvice
{
        sequential,
        random
};

struct mapped_range
{
        virtual ~mapped_range() = default;

        virtual void lock() = 0;
        virtual void unlock() = 0;
        virtual void sync() = 0;
        virtual void advise(madvice adv) = 0;

        virtual std::unique_ptr<mapped_file> 
        map_to_new_file(const char* filename) = 0;

        virtual void* data() const = 0;
        virtual std::size_t size() const = 0;
};

struct mapped_file
{
        virtual ~mapped_file() = default;

        virtual void open(const char* filename, std::ios::openmode mode) = 0;
        virtual void open(const char* filename, std::size_t size,
                std::ios::openmode mode) = 0;

        virtual bool is_open() const = 0;

        virtual std::unique_ptr<mapped_range> range(std::size_t offset,
                std::size_t size) = 0;

        virtual std::unique_ptr<mapped_range> range() = 0;

        virtual std::size_t size() const = 0;

        static std::unique_ptr<mapped_file> create();
};

class posix_mapped_range : public mapped_range
{
public:
        posix_mapped_range(void* mem, std::size_t len) noexcept;

        posix_mapped_range(posix_mapped_range&& o) noexcept;

        posix_mapped_range& operator=(posix_mapped_range&&) = delete;

        ~posix_mapped_range();

        void lock() override;

        void unlock() override;

        void sync() override;

        void advise(madvice adv) override;

        std::unique_ptr<mapped_file> map_to_new_file(const char* filename) override;

        void* data() const override;
        std::size_t size() const override;
private:
        void* mem_;
        std::size_t len_;
        bool locked_;
};

class posix_mapped_file : public mapped_file
{
public:
        posix_mapped_file();

        void open(const char* filename, std::ios::openmode mode) override;

        void open(const char* filename, std::size_t size,
                std::ios::openmode mode) override;

        ~posix_mapped_file();

        void copy(posix_mapped_file& dest);

        bool is_open() const override;

        std::unique_ptr<mapped_range> range(std::size_t offset, std::size_t size) override;

        std::unique_ptr<mapped_range> range() override;

        std::size_t size() const override;
private:
        int parse_open_mode(std::ios::openmode mode);

public:
        posix_mapped_file(posix_mapped_file&& o) noexcept;

        posix_mapped_file& operator=(posix_mapped_file&&) = delete;
private:
        int fd_;
        void* map_;
        std::size_t size_ = 0;
        std::string filename_;

};

using mapped_file_uptr = std::unique_ptr<mapped_file>;
using mapped_file_sptr = std::shared_ptr<mapped_file>;
using mapped_range_uptr = std::unique_ptr<mapped_range>;
using mapped_range_sptr = std::shared_ptr<mapped_range>;
