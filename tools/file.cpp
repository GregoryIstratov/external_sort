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
                GetLocaleInfoEx(L"en-US", LOCALE_RETURN_NUMBER | LOCALE_ILANGUAGE, (wchar_t*)&lcid, sizeof(lcid));

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
                throw_exception("Failed to read directory '"
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
                throw_exception(win_error_string()(GetLastError()));
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
                throw_exception("Failed to open directory '"
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
                throw_exception("Error in checking die existance: "
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
                throw_exception("Failed to create directory '"
                                << path << "': " << strerror(errno));
}

#endif // defined

void gen_rnd_test_file(const char* filename, uint64_t size)
{
        if (size % 4)
                throw_exception("gen_rnd_test_file: size must be product of 4");

        uint64_t N = size / 4;

        FILE *f = fopen(filename, "wb");

        if (!f) {
                throw_exception("Can't create the file '" << strerror(errno));
        }

        std::random_device rd;
        std::default_random_engine e(rd());

        std::uniform_int_distribution<uint32_t> dis;

        for (uint32_t i = 0; i < N; ++i) {
                uint32_t v = dis(e);
                fwrite(&v, 4, 1, f);
        }

        fclose(f);
}

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

        return std::move(data);
}

