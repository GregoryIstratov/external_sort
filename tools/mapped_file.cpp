#include "mapped_file.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "util.hpp"
#include "exception.hpp"

posix_mapped_range::posix_mapped_range(void* mem, std::size_t len) noexcept
        : mem_(mem), len_(len), locked_(false)
{
}

posix_mapped_range::posix_mapped_range(posix_mapped_range&& o) noexcept
        : mem_(o.mem_), len_(o.len_), locked_(o.locked_)
{
        o.mem_ = nullptr;
        o.len_ = 0;
        o.locked_ = false;
}

posix_mapped_range::~posix_mapped_range()
{
        if (locked_)
                unlock();
}

void posix_mapped_range::lock()
{
        if (mlock(mem_, len_) == -1)
                THROW_EXCEPTION << "Lock failed:" << put_errno;

        locked_ = true;
}

void posix_mapped_range::unlock()
{
        if (munlock(mem_, len_) == -1)
                THROW_EXCEPTION << "Unlock failed:" << put_errno;

        /*if(madvise(mem_, len_, MADV_REMOVE) == -1)
                THROW_EXCEPTION << "madvise failed:" << put_errno;*/

        locked_ = false;
}

void posix_mapped_range::sync()
{
        if (msync(mem_, len_, MS_SYNC) == -1)
                THROW_EXCEPTION << "Sync failed:" << put_errno;
}

std::unique_ptr<mapped_file> posix_mapped_range::map_to_new_file(
        const char* filename)
{
        std::unique_ptr<mapped_file> nf(new posix_mapped_file);
        nf->open(filename, len_, std::ios::out | std::ios::trunc);

        auto r = nf->range();
        mem_copy(r->data(), mem_, len_);

        return std::move(nf);
}

void* posix_mapped_range::data() const { return mem_; }
std::size_t posix_mapped_range::size() const { return len_; }

posix_mapped_file::posix_mapped_file()
        : fd_(-1), map_(MAP_FAILED)
{
}

void posix_mapped_file::open(const char* filename, std::ios::openmode mode)
{
        filename_ = filename;
        int o_flags = __O_NOATIME;
        o_flags |= parse_open_mode(mode);
        const int perm = DEFFILEMODE;

        fd_ = ::open(filename, o_flags, perm);

        if (fd_ == -1)
                THROW_FILE_EXCEPTION(filename_) << "Cannot open file";

        struct stat sb{};
        ::fstat(fd_, &sb);
        size_ = sb.st_size;

        int map_prot = mode & std::ios::out
                               ? PROT_READ | PROT_WRITE
                               : PROT_READ;
        int map_flags = MAP_SHARED;

        map_ = ::mmap(nullptr, sb.st_size, map_prot, map_flags, fd_, 0);

        if (map_ == MAP_FAILED)
        {
                ::close(fd_);
                fd_ = 0;

                THROW_FILE_EXCEPTION(filename_) << "mmap failed";
        }

        if (madvise(map_, size_, MADV_SEQUENTIAL) == -1)
                THROW_FILE_EXCEPTION(filename_) << "madvise error";
}

void posix_mapped_file::open(const char* filename, std::size_t size,
                             std::ios::openmode mode)
{
        size_ = size;
        filename_ = filename;

        int o_flags = __O_NOATIME;
        o_flags |= parse_open_mode(mode);
        const int perm = DEFFILEMODE;

        if ((o_flags & O_CREAT) == 0)
                THROW_FILE_EXCEPTION(filename_) << "O_CREAT must be specified";

        fd_ = ::open(filename, o_flags, perm);
        if (fd_ == -1)
                THROW_FILE_EXCEPTION(filename_) << "Cannot open file";

        if (posix_fallocate(fd_, 0, size) == -1)
                THROW_FILE_EXCEPTION(filename_) << "posix_fallocate failed:";

        int map_prot = mode & std::ios::out
                               ? PROT_READ | PROT_WRITE
                               : PROT_READ;
        int map_flags = MAP_SHARED;

        map_ = ::mmap(nullptr, size, map_prot, map_flags, fd_, 0);

        if (map_ == MAP_FAILED)
        {
                ::close(fd_);
                fd_ = 0;

                THROW_FILE_EXCEPTION(filename_) << "mmap failed";
        }

        if (madvise(map_, size_, MADV_SEQUENTIAL) == -1)
                THROW_FILE_EXCEPTION(filename_) << "madvise error";
}

posix_mapped_file::~posix_mapped_file()
{
        if (fd_ >= 0)
        {
                if (map_)
                        ::munmap(map_, size_);

                ::close(fd_);
        }
}

void posix_mapped_file::copy(posix_mapped_file& dest)
{
        mem_copy(dest.map_, map_, size_);
}

std::unique_ptr<mapped_range> posix_mapped_file::range(
        std::size_t offset, std::size_t size)
{
        if (!this->is_open())
                THROW_EXCEPTION << "File " << quote(filename_) <<
                        " is not open";

        auto p = new posix_mapped_range((char*)map_ + offset, size);

        return std::unique_ptr<mapped_range>(p);
}

std::unique_ptr<mapped_range> posix_mapped_file::range()
{
        return this->range(0, size_);
}

std::size_t posix_mapped_file::size() const { return size_; }

int posix_mapped_file::parse_open_mode(std::ios::openmode mode)
{
        int o_flags = 0;

        if ((mode & std::ios::out) && (mode & std::ios::in))
        {
                o_flags |= O_RDWR | O_CREAT;
        }
        else
        {
                if (test_and_clear(&mode, std::ios::in))
                {
                        o_flags |= O_RDONLY;
                }
                else if (test_and_clear(&mode, std::ios::out))
                {
                        o_flags |= O_RDWR | O_CREAT;
                }
        }

        if (test_and_clear(&mode, std::ios::trunc))
        {
                o_flags |= O_TRUNC;
        }

        return o_flags;
}

posix_mapped_file::posix_mapped_file(posix_mapped_file&& o) noexcept
        : fd_(o.fd_),
          map_(o.map_),
          size_(o.size_),
          filename_(std::move(o.filename_))
{
        o.fd_ = -1;
        o.map_ = MAP_FAILED;
        o.size_ = 0;
}

bool posix_mapped_file::is_open() const
{
        return fd_ != -1 && map_ != MAP_FAILED;
}


std::unique_ptr<mapped_file> mapped_file::create()
{
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
        return std::unique_ptr<mapped_file>(new posix_mapped_file);
#else
        THROW_EXCEPTION << "Not implemented";
#endif
}
