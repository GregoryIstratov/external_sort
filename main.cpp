/* =====================================================
 *                    EXTERNAL SORT
 * =====================================================
 * This program is for sorting very large amount of data
 * that cannot fit into machine's RAM by splitting input
 * file into small pieces that fit into main memory
 * sorting and merging them into one file.
 *
 * =====================================================
 *               MULTITHREADED PIPELINE
 * =====================================================
 *                      STAGE 1
 *
 *              Sort chunks and save them
 * =====================================================
 * -----------------------------------------------------
 *      Thread 1     |    Thread 2     |     Thread N  |
 * -----------------------------------------------------
 *      Get Task     |    Get Task     |    Get Task   |
 *      Sort         |    Sort         |    Sort       |
 *      Save Data    |    Save Data    |    Save Data  |
 * -----------------------------------------------------
 * =====================================================
 *                  Synchronize threads
 * =====================================================
 *                      STAGE 2
 *                    NO FLAT MODE
 *        Merge sorted N chunks -> 1, save it
 *        go to the next level and repeat
 * =====================================================
 *
 *          T1           |           T2
 * _______________________________________________
 * С1    С2    С3    С4  |  С5    С6    С7    С8 |
 *   \  /        \  /    |    \  /        \  /   |
 *    C21         C22    |     C23         C24   |
 *      \        /       |       \        /      |
 *       \      /        |        \      /       |
 *        \    /         |         \    /        |
 * _______________________________________________
 *          \/           T1          \/
 *          C31                      C32
 *             \                    /
 *              \                  /
 *               \                /
 *                \ ____________ /
 *                      \ /
 *                     RESULT
  * =====================================================
 *                      STAGE 2
 *                     FLAT MODE
 *           Merge sorted ALL chunks -> 1
 * =====================================================
 *
 *   С1    С2    С3    С4   С5    С6    С7    С8   Cn
 *    |    |     |     |    |     |     |     |     |
 *    -----------------------------------------------
 *    |        N-WAY MERGE WITH PRIORITY QUEUE      |
 *    -----------------------------------------------
 *    |    |     |     |    |     |     |     |     |
 *    |    |     |     |    |     |     |     |     |
 *    _______________________________________________
 *    |                   RESULT                    |
 *    _______________________________________________
 *
 * =====================================================
 *                    Save result
 * =====================================================
 */

#include <cmath>
#include <type_traits>
#include <iterator>

#include "task.hpp"
#include "pipeline/pipeline_controller.hpp"
#include "log.hpp"
#include "extra/hasher.hpp"
#include "chunk/chunk_istream.hpp"

float solve_merge_n_eq2(float a, float c)
{
        if(c < 2.0)
                c = 2.0;

        return std::pow(a, 1.0f/c);
}

size_t get_thread_number()
{
        auto n = std::thread::hardware_concurrency();

        return n != 0 ? n : CONFIG_DEFAULT_THREAD_NUMBER;
}

uint32_t get_nway_merge_n(uint64_t cn)
{
        if(IS_ENABLED(CONFIG_N_WAY_FLAT))
                return (uint32_t)cn;

        if(IS_ENABLED(CONFIG_N_WAY_MERGE_N))
                return (uint32_t)CONFIG_N_WAY_MERGE_N;

        return (uint32_t)std::round(solve_merge_n_eq2((float)cn,
                                                      CONFIG_TREE_HEIGH)
        );
}

void check_result(uint64_t isz)
{
        if (!IS_ENABLED(CONFIG_CHECK_HASH))
        {
                chunk_istream<CONFIG_DATA_TYPE> res_is;
                res_is.open(CONFIG_OUTPUT_FILENAME, CONFIG_MEM_AVAIL);

                uint64_t sz = res_is.size();
                if (isz == sz)
                        info2() << "Input filesize " << isz << "=="
                                << sz << " output filesize";
                else
                        error() << "Input filesize " << isz << "!="
                                << sz << " output filesize";

                info() << "Checking is file sorted...";

                chunk_istream_iterator<CONFIG_DATA_TYPE> beg(res_is), end;
                if (std::is_sorted(beg, end))
                        info() << "File is sorted";
                else
                        error() << "File is NOT sorted";
        }
        else
        {
                hash_value<8> hash(crc64_from_file(CONFIG_OUTPUT_FILENAME));

                auto data = file_read_all(CONFIG_ORIGIN_HASH_FILENAME);
                hash_value<8> origin_hash(&data[0]);

                if (hash == origin_hash)
                        info() << "File is sorted";
                else
                        error() << "File is NOT sorted origin hash "
                        << origin_hash << " output hash " << hash;
        }
}

void print_result()
{
        chunk_istream<CONFIG_DATA_TYPE> is;
        is.open(CONFIG_OUTPUT_FILENAME, CONFIG_MEM_AVAIL);

        chunk_istream_iterator<CONFIG_DATA_TYPE> beg(is), end;

        std::stringstream ss;

        ss << "Result [";
        std::copy(beg, end, std::ostream_iterator<CONFIG_DATA_TYPE>(ss, " "));
        ss << "]" << std::endl;

        info() << ss.rdbuf();
}

void make_test_file()
{
        static_assert(!(CONFIG_TEST_FILE_TYPE == CONFIG_TEST_FILE_RANDOM
                        && CONFIG_CHECK_HASH),
                "CONFIG_TEST_FILE_RANDOM and CONFIG_CHECK_HASH"
                " cannot be used together");

        switch (CONFIG_TEST_FILE_TYPE)
        {
        case CONFIG_TEST_FILE_SHUFFLE:
        {
                using logging::fmt_clear;
                using logging::fmt_set;
                using logging::fmt;

                info2() << "Generating " << size_format(CONFIG_TEST_FILE_SIZE)
                        << " test data..";

                std::vector<CONFIG_DATA_TYPE> arr(CONFIG_TEST_FILE_SIZE
                        / sizeof(CONFIG_DATA_TYPE));

                auto i = CONFIG_DATA_TYPE();

                for (auto& v : arr)
                        v = i++;

                info2() << "Computing hash of test data..."
                        << fmt_clear(fmt::endl);

                hasher_crc64 hasher;
                hasher.put(arr);
                auto hash = hasher.hash();

                info2() << fmt_set(fmt::append) << ": " << hash;

                file_write(CONFIG_ORIGIN_HASH_FILENAME, hash.data(), hash.size());

                info2() << "Writing test file...";

                make_rnd_file_from(arr, CONFIG_INPUT_FILENAME);
                break;
        }

        case CONFIG_TEST_FILE_RANDOM:
        {
                gen_rnd_test_file(CONFIG_INPUT_FILENAME,
                        CONFIG_TEST_FILE_SIZE);
                break;
        }

        default:
                throw_exception("Unknown test file type");
        }
}

void init_enviroment()
{
        if (!check_dir_exist(CONFIG_CHUNK_DIR))
                create_directory(CONFIG_CHUNK_DIR);
}

int main(int argc, char** argv)
try
{
        logging::logger::enable_file_logging("external_sort.log");

        init_enviroment();

        std::string input_filename = CONFIG_INPUT_FILENAME;

        if (IS_ENABLED(CONFIG_GENERATE_TEST_FILE))
        {
                perf_timer("Test file generating:", &make_test_file);
        }
        else if (argc > 1)
                input_filename = argv[1];                
        

        using data_t = CONFIG_DATA_TYPE;

        raw_file_reader rfr_(input_filename);

        size_t threads_n = get_thread_number();

        uint64_t input_filesize  = rfr_.file_size();
        uint64_t ncpu = threads_n;
        uint64_t mem_avail  = CONFIG_MEM_AVAIL;
        uint64_t thr_mem = mem_avail / ncpu;
        uint64_t l0_chunk_size  = thr_mem;

        if(mem_avail >= input_filesize)
                l0_chunk_size = input_filesize / (threads_n * 2);

        uint64_t chunk_number   = input_filesize / l0_chunk_size;
        uint32_t merge_n = get_nway_merge_n(chunk_number);

        /* 2-way is a minimum requirement */
        merge_n = merge_n <= 1 ? 2 : merge_n;

        float io_ratio  = CONFIG_IO_BUFF_RATIO;
        auto input_buff_size = static_cast<uint64_t>(thr_mem * io_ratio
                                                     / (float)merge_n);

        auto output_buff_size = static_cast<uint64_t>(thr_mem * (1.0f - io_ratio));

        input_buff_size = round_up(input_buff_size, sizeof(data_t));
        output_buff_size = round_up(output_buff_size, sizeof(data_t));

        info() << "Input Filename: " << input_filename;
        info() << "Output Filename: " << CONFIG_OUTPUT_FILENAME;
        info() << "Input Filesize: " << size_format(input_filesize);
        info() << "Threads : " << threads_n;
        info() << "MEM Available: " << size_format(mem_avail);
        info() << "MEM Per Thread: " << size_format(thr_mem);
        info() << "MEM IO Ratio: " << io_ratio;
        info() << "K-way Merge Size: " << merge_n;
        info() << "IChunk Buff Size: " << size_format(input_buff_size);
        info() << "OChunk Buff Size: " << size_format(output_buff_size);
        info() << "L0 Chunk Size: " << size_format(l0_chunk_size);
        info() << "L0 Chunk Count: " << num_format(chunk_number);

        /*check constrains */
        if (input_buff_size < sizeof(data_t))
                throw_exception("Input buffer size is too small = " 
                                << size_format(input_buff_size));

        if (output_buff_size < sizeof(data_t))
                throw_exception("Output buffer size is too small = "
                        << size_format(output_buff_size));

        /* main unit in the program */
        pipeline_controller<data_t> controller(
                              std::move(rfr_),
                              l0_chunk_size, merge_n, 
                              (uint32_t)threads_n, 
                              mem_avail, 
                              io_ratio,
                              CONFIG_OUTPUT_FILENAME
        );

        perf_timer("Finished for", [&controller](){
                controller.run();
        });

        if (IS_ENABLED(CONFIG_PRINT_RESULT))
                print_result();

        if(IS_ENABLED(CONFIG_CHECK_RESULT))
                check_result(input_filesize);

        if (IS_ENABLED(CONFIG_REMOVE_RESULT))
                delete_file(CONFIG_OUTPUT_FILENAME);

        return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
        error() << e.what();

        return EXIT_FAILURE;
}
