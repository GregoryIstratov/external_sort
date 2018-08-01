/* =====================================================
 *                    EXTERNAL SORT
 * =====================================================
 * This program is for sorting very large amount of data
 * that cannot fit into machine's RAM by splitting input
 * file into small pieces that fit into main memory
 * sorting and merging them into one file.
 * 
 * =====================================================
 *                FORMULAS OF COMPUTATION
 * =====================================================
 *
 * RAM  - Ram Size ( 128 Mb default )
 * ISZ  - input file size > RAM.
 * NCPU - Number of CPU threads ( auto or 2 default )
 * TRAM - Ram per thread - RAM / NCPU
 * MLVL - Max level = 2
 * CS0  - L0 Chunk Size   = TRAM
 * CN   - L0 Chunk Number = ISZ / CS0
 * NMRG - N-Way merge, solution for log(CN)/log(x) = MLVL = CN^(1/MLVL)
 * IOR  - In/out TRAM ratio - default 0.5
 * IBSZ - In Buffer Size per chunk = TRAM * IOR / NMRG
 * OBSZ - Out buffer size per task = TRAM * (1 - IOR) / NMRG
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

#include "chunk.hpp"
#include "util.hpp"
#include "task_tree.hpp"
#include "task.hpp"
#include "pipeline.hpp"

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

        return (uint32_t)std::round(solve_merge_n_eq2((float)cn, CONFIG_TREE_HEIGH));
}

void check_result(uint64_t isz)
{
        chunk_istream<CONFIG_DATA_TYPE> res_is(CONFIG_OUTPUT_FILENAME);
        res_is.open(CONFIG_MEM_AVAIL);

        uint64_t sz = res_is.size();
        if(isz == sz)
                info2() << "Input filesize " << isz << "==" <<sz<<" output filesize";
        else
                error() << "Input filesize " << isz << "!=" <<sz<<" output filesize";

        info() << "Checking is file sorted...";

        chunk_istream_iterator<CONFIG_DATA_TYPE> beg(res_is), end;
        if(std::is_sorted(beg, end))
                info() << "File is sorted";
        else
                error() << "File is NOT sorted";
}

void print_result()
{
        chunk_istream<CONFIG_DATA_TYPE> is(CONFIG_OUTPUT_FILENAME);
        is.open(CONFIG_MEM_AVAIL);

        chunk_istream_iterator<CONFIG_DATA_TYPE> beg(is), end;

        std::cout << "Result [";
        std::copy(beg, end, std::ostream_iterator<CONFIG_DATA_TYPE>(std::cout, " "));
        std::cout << "]" << std::endl;
}

void make_test_file()
{
        switch(CONFIG_TEST_FILE_TYPE)
        {
                case CONFIG_TEST_FILE_SHUFFLE:
                {
                        std::vector<CONFIG_DATA_TYPE> arr(CONFIG_TEST_FILE_SIZE
                                                          / sizeof(CONFIG_DATA_TYPE));

                        CONFIG_DATA_TYPE i = CONFIG_DATA_TYPE();

                        for(auto& v : arr)
                                v = i++;

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

int main()
try
{
        logger::enable_file_logging("external_sort.log");

        if(IS_ENABLED(CONFIG_GENERATE_TEST_FILE))
                perf_timer("Test file generating", &make_test_file);

        using data_t = CONFIG_DATA_TYPE;

        raw_file_reader rfr_(CONFIG_INPUT_FILENAME);

        size_t threads_n = get_thread_number();

        uint64_t isz  = rfr_.file_size();
        uint64_t ncpu = threads_n;
        uint64_t ram  = CONFIG_MEM_AVAIL;
        uint64_t tram = ram / ncpu;
        uint64_t cs0  = tram;

        if(ram >= isz)
                cs0 = isz / (threads_n * 2);

        uint64_t cn   = isz / cs0;
        uint32_t nmrg = get_nway_merge_n(cn);

        // 2-way is min requirement
        nmrg = nmrg <= 1 ? 2 : nmrg;

        float ior  = CONFIG_IO_BUFF_RATIO;
        uint64_t ibsz = static_cast<uint64_t>(tram * ior / (float)nmrg);
        uint64_t obsz = static_cast<uint64_t>(tram * (1.0f - ior));

        ibsz = round_up(ibsz, (uint64_t)sizeof(data_t));
        obsz = round_up(obsz, (uint64_t)sizeof(data_t));

        size_t chunk_size = cs0;
        size_t in_buff_size = ibsz;
        size_t out_buff_size = obsz;
        size_t n_way_merge = nmrg;


        info() << "Input Filename: " << CONFIG_INPUT_FILENAME;
        info() << "Output Filename: " << CONFIG_OUTPUT_FILENAME;
        info() << "Input Filesize: " << size_format(isz);
        info() << "Threads : " << threads_n;
        info() << "MEM Available: " << size_format(ram);
        info() << "MEM Per Thread: " << size_format(tram);
        info() << "MEM IO Ratio: " << ior;
        info() << "K-way Merge Size: " << n_way_merge;
        info() << "IChunk Buff Size: " << size_format(in_buff_size);
        info() << "OChunk Buff Size: " << size_format(out_buff_size);
        info() << "L0 Chunk Size: " << size_format(cs0);
        info() << "L0 Chunk Count: " << num_format(cn);

        processor<data_t> pcr(std::move(rfr_),
                              chunk_size, n_way_merge, 
                              (uint32_t)threads_n, 
                              ram, 
                              ior,
                              CONFIG_OUTPUT_FILENAME);

        perf_timer("Finished for", [&pcr](){
                pcr.run();
        });


        if(IS_ENABLED(CONFIG_CHECK_RESULT))
                check_result(isz);

        if(IS_ENABLED(CONFIG_PRINT_RESULT))
                print_result();

        return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
        error() << e.what();

        return EXIT_FAILURE;
}
