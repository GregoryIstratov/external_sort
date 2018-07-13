#include "file_io.hpp"

void file_write(const char* filename, const void* data, size_t size)
{
        FILE* f = fopen(filename, "wb");

        if(!f)
        {
                throw std::runtime_error(
                        "Can't open the file '"
                        + std::string(filename) + "' :"
                        + strerror(errno));
        }

        fwrite(data, 1, size, f);

        if(ferror(f))
        {
                fclose(f);

                throw std::runtime_error(
                        std::string("Can't write the file '")
                        + filename + "' : "
                        + strerror(errno));

        }

        fclose(f);
}