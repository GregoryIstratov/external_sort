#include "util.hpp"
#include <random>

std::mutex logger::mtx_;


void gen_rnd_test_file(const char* filename, uint64_t size)
{
        if (size % 4)
                throw std::runtime_error("gen_rnd_test_file: size must be product of 4");

        uint64_t N = size / 4;

        FILE *f = fopen(filename, "wb");

        if (!f) {
                throw std::runtime_error(
                        std::string("Can't create the file '")
                        + strerror(errno));
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
