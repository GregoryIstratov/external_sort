#include <list>

#include "file.hpp"
#include "exception.hpp"

#if defined(_WINDOWS)

#include <Windows.h>

class win_error_string
{
public:
        ~win_error_string()
        {
                if (err_)
                {
                        LocalFree(err_);
                        err_ = nullptr;
                }
        }

        std::string operator()(DWORD err_code)
        {
                LCID lcid;
                GetLocaleInfoEx(L"en-US", LOCALE_RETURN_NUMBER 
                                        | LOCALE_ILANGUAGE, 
                                (wchar_t*)&lcid, sizeof(lcid));

                if (!FormatMessageA(
                        FORMAT_MESSAGE_ALLOCATE_BUFFER
                        | FORMAT_MESSAGE_FROM_SYSTEM,
                        nullptr,
                        err_code,
                        lcid,
                        (LPTSTR)&err_,
                        0,
                        nullptr)
                        )
                {
                        return std::string();
                }

                return std::string(err_);
        }

private:
        char* err_ = nullptr;

};

void delete_file(const char* filename)
{
        if (!DeleteFileA(filename))
        {
                auto s = win_error_string()(GetLastError());

                throw std::runtime_error(s);
        }
}

void iterate_dir(const char* path, std::function<void(const char*)>&& callback)
{
        std::list<std::string> names;

        std::string search_path = path;
        search_path += "/*.*";

        WIN32_FIND_DATA fd;
        HANDLE h_find = FindFirstFileA(search_path.c_str(), &fd);

        if (h_find != INVALID_HANDLE_VALUE)
        {
                do
                {
                        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                        {
                                callback(fd.cFileName);
                        }

                } while (FindNextFileA(h_find, &fd));

                FindClose(h_find);
        }
        else
        {
                THROW_EXCEPTION("Failed to read directory '"
                        << path
                        << "': "
                        << win_error_string()(GetLastError()));
        }
}

bool check_dir_exist(const char* path)
{
        DWORD dw_attrib = GetFileAttributesA(path);

        return (dw_attrib != INVALID_FILE_ATTRIBUTES &&
                (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
}

void create_directory(const char* name)
{
        if (!CreateDirectoryA(name, nullptr))
                THROW_EXCEPTION(win_error_string()(GetLastError()));
}

#else

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

void delete_file(const char* filename)
{
        if (std::remove(filename) != 0)
                throw std::runtime_error(strerror(errno));
}

void iterate_dir(const char* path, std::function<void(const char*)>&& callback)
{
        struct dir_guard
        {
                explicit
                dir_guard(DIR* _dir) : p_dir(_dir) {}

                ~dir_guard()
                {
                        if(p_dir) closedir(p_dir);
                }

                DIR* p_dir;
        };

        DIR* dir = opendir(path);
        dirent* ent;

        dir_guard guard(dir);

        if(!dir)
                THROW_EXCEPTION("Failed to open directory '"
                                        << path
                                        << "': "
                                        << strerror(errno));


        while((ent = readdir(dir)))
        {
                if (std::strcmp(ent->d_name, ".") == 0
                    || std::strcmp(ent->d_name, "..") == 0)
                        continue;

                callback(ent->d_name);
        }
}

bool check_dir_exist(const char* path)
{
        DIR* dir = opendir(path);
        if (dir)
        {
                closedir(dir);
                return true;
        }
        else if (ENOENT == errno)
        {
                return false;
        }
        else
        {
                THROW_EXCEPTION("Error in checking die existance: "
                                << strerror(errno));
        }
}

void create_directory(const char* path)
{
#if defined(_WIN32)
        auto ret = mkdir(path);
#else
        mode_t mode = 0755;
        auto ret = mkdir(path, mode);
#endif

        if(ret != 0)
                THROW_EXCEPTION("Failed to create directory '"
                                << path << "': " << strerror(errno));
}

#endif // defined

void _file_write(std::string&& filename, const void* data, size_t size)
{
        raw_file_writer f(std::move(filename));

        f.write(data, size);
}

std::vector<unsigned char> _file_read_all(std::string&& filename)
{
        raw_file_reader f(std::move(filename));

        uint64_t sz = f.file_size();

        std::vector<unsigned char> data(sz);

        f.read((char*)&data[0], sz);

        return data;
}

class boost_memory_mapped_file_source;
class boost_memory_mapped_file_sink;

class system_mmap_file_source {};
class system_mmap_file_sink {};

/* Little explanation on this:
 * If we want to use memory mapping we could choose between using boost maping 
 * and OS API maping, if CONFIG_USE_MMAP is enabled and boost is disabled or 
 * not available we check for the system mmap which can be not implemented for 
 * the host and we throw static_assert to not let it be compiled.
 */

template<typename T>
struct mmap_source_factory
{
        //static_assert(!IS_ENABLED(CONFIG_USE_MMAP),
        //              "Unsupported mmap type for this OS");

        std::unique_ptr<memory_mapped_file_source> operator()() const
        {
                throw;
        }
};

template<typename T>
struct mmap_sink_factory
{
        //static_assert(!IS_ENABLED(CONFIG_USE_MMAP), 
        //              "Unsupported mmap type for this OS");

        std::unique_ptr<memory_mapped_file_sink> operator()() const
        {
                throw;
        }
};

#if defined(__BOOST_FOUND)
#include <boost/iostreams/device/mapped_file.hpp>

class boost_memory_mapped_file_source : public memory_mapped_file_source
{
public:
        void open(const char* filename) override
        {
                source_.open(filename);

                if (!source_)
                        THROW_EXCEPTION("Cannot open file '"
                                << filename << "' for mapping");
        }

        void close() override
        {
                source_.close();
        }

        size_t size() const override
        {
                return source_.size();
        }

        const char* data() const override
        {
                return source_.data();
        }
private:
        boost::iostreams::mapped_file_source source_;
};

class boost_memory_mapped_file_sink : public memory_mapped_file_sink
{
public:
        void open(const char* filename, size_t size, 
                  size_t offset, bool create) override
        {
                boost::iostreams::mapped_file_params params;
                params.path = filename;
                params.flags = boost::iostreams::mapped_file_base::readwrite;
                params.offset = offset;

                if (!create)
                        params.length = size;
                else
                        params.new_file_size = size;

                sink_.open(params);
                if (!sink_)
                        THROW_EXCEPTION("Can't open file '" << filename 
                                        << "' for mapping");
        }

        void close() override
        {
                sink_.close();
        }

        char* data() const override
        {
                return sink_.data();
        }
private:
        boost::iostreams::mapped_file_sink sink_;
};

template<>
struct mmap_source_factory<boost_memory_mapped_file_source>
{
        auto operator()() const
        {
                return std::make_unique<boost_memory_mapped_file_source>();
        }
};

template<>
struct mmap_sink_factory<boost_memory_mapped_file_sink>
{
        auto operator()() const
        {
                return std::make_unique<boost_memory_mapped_file_sink>();
        }
};

#endif // __BOOST_FOUND

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))


#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class posix_mmap_file_source : public memory_mapped_file_source
{
public:
        ~posix_mmap_file_source()
        {
                close();
        }

        void open(const char* filename) override
        {
                auto fd = ::open(filename, O_RDONLY);
                if (fd == -1)
                        THROW_EXCEPTION("Cannot open file '" << filename
                                << "': " << strerror(errno));

                struct stat sb;
                if (fstat(fd, &sb) == -1)
                        THROW_EXCEPTION("Cannot get file '" << filename
                                << "' stat: " << strerror(errno));

                size_ = sb.st_size;

                ptr_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);

                if (ptr_ == MAP_FAILED)
                        THROW_EXCEPTION("mmap file '" << filename
                                << "' failed: " << strerror(errno));
        }

        size_t size() const override
        {
                return size_;
        }

        void close() override
        {
                if (ptr_ != nullptr || ptr_ != MAP_FAILED)
                {
                        munmap(ptr_, size_);
                        ptr_ = nullptr;
                }

                if (fd_ != fd_type())
                {
                        ::close(fd_);
                        fd_ = fd_type();
                }
        }

        const char* data() const override
        {
                return (char*)ptr_;
        }
private:
        using fd_type = decltype(::open("", O_RDONLY));

        fd_type fd_ = fd_type();
        void* ptr_ = nullptr;
        size_t size_ = 0;
};

class posix_mmap_file_sink : public memory_mapped_file_sink
{
public:
        ~posix_mmap_file_sink()
        {
                close();
        }

        void open(const char* filename, size_t size,
                size_t offset, bool create) override
        {
                size_ = size;

                fd_ = ::open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

                if (fd_ == -1)
                        THROW_EXCEPTION("Cannot open file '" << filename
                                << "': " << strerror(errno));

                /* Something needs to be written at the end of the file to
                * have the file actually have the new size.
                * Just writing an empty string at the current file position will do.
                *
                * Note:
                *  - The current position in the file is at the end of the stretched
                *    file due to the call to lseek().
                *  - An empty string is actually a single '\0' character, so a zero-byte
                *    will be written at the last byte of the file.
                */

                if (lseek(fd_, size - 1, SEEK_SET) == -1)
                        THROW_EXCEPTION("Cannot seek file '" << filename 
                                        << "': " << strerror(errno));

                if (write(fd_, "", 1) == -1)
                        THROW_EXCEPTION("Cannot write to file '" << filename 
                                        << "': " << strerror(errno));

                ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                            fd_, 0);

                if (ptr_ == MAP_FAILED)
                        THROW_EXCEPTION("Cannot map file '" << filename
                                << "': " << strerror(errno));
        }

        void close() override
        {
                if (ptr_ != nullptr || ptr_ != MAP_FAILED)
                {
                        munmap(ptr_, size_);
                        ptr_ = nullptr;
                }

                if (fd_ != fd_type())
                {
                        ::close(fd_);
                        fd_ = fd_type();
                }
        }

        char* data() const override
        {
                return (char*)ptr_;
        }

private:
        using fd_type = decltype(::open("", O_RDONLY));

        fd_type fd_ = fd_type();
        void* ptr_ = nullptr;
        size_t size_ = 0;
};

template<>
struct mmap_source_factory<system_mmap_file_source>
{
        auto operator()() const
        {
                return std::make_unique<posix_mmap_file_source>();
        }
};

template<>
struct mmap_sink_factory<system_mmap_file_sink>
{
        auto operator()() const
        {
                return std::make_unique<posix_mmap_file_sink>();
        }
};

#endif // POSIX

/*
 * If boost if not enabled or not available fallback to system mmap
 */

using mmap_source_boost_type = std::conditional<IS_ENABLED(CONFIG_BOOST), 
                                 boost_memory_mapped_file_source, 
                                 system_mmap_file_source>::type;


using mmap_sink_boost_type = std::conditional<IS_ENABLED(CONFIG_BOOST),
                                boost_memory_mapped_file_sink,
                                system_mmap_file_sink>::type;

using mmap_source_type = std::conditional<IS_ENABLED(CONFIG_PREFER_BOOST_MMAP), 
                              mmap_source_boost_type,
                              system_mmap_file_source>::type;

using mmap_sink_type = std::conditional<IS_ENABLED(CONFIG_PREFER_BOOST_MMAP),
                            mmap_sink_boost_type,
                            system_mmap_file_sink>::type;

std::unique_ptr<memory_mapped_file_source> memory_mapped_file_source::create()
{
        return mmap_source_factory<mmap_source_type>()();
}

std::unique_ptr<memory_mapped_file_sink> memory_mapped_file_sink::create()
{
        return mmap_sink_factory<mmap_sink_type>()();
}
