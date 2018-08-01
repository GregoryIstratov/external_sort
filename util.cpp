#include "util.hpp"
#include <stdexcept>
#include <cstdio>
#include <random>
#include <algorithm>

#if defined(_WINDOWS)

#include <Windows.h>

class win_error_string
{
public:
        ~win_error_string()
        {
                if(err_)
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
        if(!DeleteFileA(filename))
        {
                auto s = win_error_string()(GetLastError());

                throw std::runtime_error(s);
        }
}

#else

void delete_file(const char* filename)
{
        if (std::remove(filename) != 0)
                throw std::runtime_error(strerror(errno));
}

#endif

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

void file_write(const char* filename, const void* data, size_t size)
{
        raw_file_writer f(filename);

        f.write(data, size);
}
