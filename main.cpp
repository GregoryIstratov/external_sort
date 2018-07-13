/* =====================================================
 *                    EXTERNAL SORT
 * =====================================================
 * This program is for sorting ANY amount of data
 * on machines with very low amount of RAM on board.
 * Like a machine with 128 MiB RAM and enough disk space
 * on board can sort 1TB ( or more ) file of 32-bit integers.
 *
 * =====================================================
 *                FORMULAS OF COMPUTATION
 * =====================================================
 *
 * RAM  - Ram Size ( 128 Mb default )
 * ISZ  - input file size > RAM.
 * NCPU - Number of CPU threads ( auto or 2 default )
 * TRAM - Ram per thread - RAM / NCPU
 * MLVL - Max level = 5
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
 *
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
 *
 * =====================================================
 *                   Save result
 * =====================================================
 */

#include <cmath>
#include <type_traits>

#include "file_io.hpp"
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


#if !CONFIG_N_WAY_MERGE_N
uint32_t  get_nway_merge_n(uint64_t cn)
{
        return (uint32_t)std::round(solve_merge_n_eq2(cn, CONFIG_TREE_HEIGH));
}
#else
uint32_t get_nway_merge_n(uint64_t)
{
        return (uint32_t)CONFIG_N_WAY_MERGE_N;
}
#endif

#if CONFIG_CHECK_RESULT
void check_result(uint64_t isz)
{
        chunk_istream<CONFIG_DATA_TYPE> res_is(CONFIG_OUTPUT_FILENAME);
        res_is.open(CONFIG_MEM_AVAIL);
        if(isz == res_is.size())
                info2() << "Input filesize " << isz << "==" <<res_is.size()<<" output filesize";
        else
                info2() << "Input filesize " << isz << "!=" <<res_is.size()<<" output filesize";

        info() << "Checking is file sorted...";
        chunk_istream_iterator<CONFIG_DATA_TYPE> beg(res_is), end;
        if(std::is_sorted(beg, end))
                info() << "File is sorted";
        else
                info() << "File is NOT sorted";
}
#else
void check_result(uint64_t isz) {}
#endif

int main()
try
{
        //gen_rnd_test_file(CONFIG_INPUT_FILENAME, 500 * MEGABYTE);
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
        uint64_t ibsz = std::floor(tram * ior / (float)nmrg);
        uint64_t obsz = std::floor(tram * (1.0f - ior));

        ibsz = round_up(ibsz, sizeof(data_t));
        obsz = round_up(obsz, sizeof(data_t));

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

        pipeline<data_t> pl(std::move(rfr_),
                            chunk_size, n_way_merge, threads_n, ram, ior,
                            CONFIG_OUTPUT_FILENAME);

        perf_timer tm;

        tm.start();

        pl.run();

        tm.end();

        info() << "Finished for " << tm.elapsed<perf_timer::ms>() << " ms";


        check_result(isz);

        return 0;
}
catch (const std::exception& e)
{
        std::cerr << "ERROR: " << e.what() << std::endl;
}
