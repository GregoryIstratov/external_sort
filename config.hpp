/* ****************************************************************************
* SETTINGS & GLOBALS
* ***************************************************************************/

#pragma once
#include <cstdint>
#include <cstddef>
#include "tools/literals.hpp"

namespace config
{
class option
{
public:
        explicit constexpr option(bool enabled)
                           : enabled_(enabled) {}

        friend constexpr bool IS_ENABLED(option _option)
        {
                return _option.enabled_;
        }

        friend constexpr bool IS_DISABLED(option _option)
        {
                return !_option.enabled_;
        }
private:
        const bool enabled_;
};

constexpr option depends_on(option opt, option value)
{
        return option(IS_ENABLED(opt) && IS_ENABLED(value));
}

constexpr option operator ||(option a, option b)
{
        return option(IS_ENABLED(a) || IS_ENABLED(b));
}

constexpr option operator &&(option a, option b)
{
        return option(IS_ENABLED(a) && IS_ENABLED(b));
}

constexpr option conflicts_with(option opt, option value)
{
        return option(!IS_ENABLED(opt) && IS_ENABLED(value));
}

constexpr option ON(true);
constexpr option OFF(false);
}

/******************************************************************************
* COMMON SECTION
*****************************************************************************/

using CONFIG_DATA_TYPE = uint32_t;

/* Number of threads if hardware_concurrency() fails */
constexpr size_t CONFIG_DEFAULT_THREAD_NUMBER = 2;

#if defined(__BOOST_FOUND)
constexpr auto CONFIG_BOOST = config::ON;
#else
constexpr auto CONFIG_BOOST = config::OFF;
#endif

constexpr auto CONFIG_FORCE_DEBUG = config::ON;

#if defined(_NDEBUG)
constexpr auto CONFIG_DEBUG = config::OFF || CONFIG_FORCE_DEBUG;
#else
constexpr auto CONFIG_DEBUG = config::ON;
#endif

/******************************************************************************
* FILE SECTION
*****************************************************************************/

constexpr const char* CONFIG_INPUT_FILENAME  = "input";
constexpr const char* CONFIG_OUTPUT_FILENAME = "output";
constexpr const char  CONFIG_CHUNK_NAME_SEP = '_';
constexpr const char* CONFIG_CHUNK_DIR = "chunks";

constexpr auto CONFIG_REMOVE_TMP_FILES = config::OFF;

constexpr auto CONFIG_USE_MMAP = config::ON;

constexpr auto CONFIG_PREFER_BOOST_MMAP = config::OFF;

constexpr auto CONFIG_USE_CPP_STREAMS = config::ON;

/******************************************************************************
* SORT SECTION
*****************************************************************************/

enum
{
        CONFIG_SORT_HEAP,
        CONFIG_SORT_STD,
        CONFIG_SORT_RADIX,
        CONFIG_SORT_BLOCK_INDIRECT
};

constexpr int CONFIG_SORT_ALGO = CONFIG_SORT_BLOCK_INDIRECT;

constexpr auto CONFIG_SORT_PARALLEL = config::option(CONFIG_SORT_ALGO 
                                                == CONFIG_SORT_BLOCK_INDIRECT);

/******************************************************************************
* MERGE SECTION
*****************************************************************************/

constexpr auto CONFIG_N_WAY_FLAT = config::ON;

/* 0 - auto, n > 2 = n */
constexpr int CONFIG_N_WAY_MERGE_N = 0;

constexpr int CONFIG_TREE_HEIGH = 2;

/******************************************************************************
* MEMORY SECTION
*****************************************************************************/

/* Default page size on most systems */
constexpr size_t PAGE_SIZE = 4096;

/* -x MiB for program itself and some part of each thread stack*/
constexpr size_t CONFIG_MEM_AVAIL = 128_MiB;
//constexpr size_t CONFIG_MEM_AVAIL = 512;

constexpr float CONFIG_IO_BUFF_RATIO = 0.3f;

/******************************************************************************
* LOG SECTION
*****************************************************************************/

constexpr auto CONFIG_PERF_MEASURE_GET_NEXT_SORT_TASK = config::OFF;

constexpr int CONFIG_INFO_LEVEL = 2;

constexpr auto CONFIG_ENABLE_STACKTRACE = config::OFF;


/******************************************************************************
 * TEST SECTION
 *****************************************************************************/

constexpr auto CONFIG_GENERATE_TEST_FILE = config::ON;

constexpr auto CONFIG_SKIP_SORT = config::conflicts_with(
                                        CONFIG_GENERATE_TEST_FILE, config::OFF);

constexpr auto CONFIG_REMOVE_RESULT = config::ON;

constexpr auto CONFIG_CHECK_RESULT = config::ON;

constexpr auto CONFIG_CHECK_HASH = config::ON;

constexpr const char* CONFIG_ORIGIN_HASH_FILENAME = "origin.hash";

constexpr auto CONFIG_PRINT_RESULT = config::OFF;

constexpr auto CONFIG_PRINT_CHUNK_DATA = config::OFF;

enum
{
        CONFIG_TEST_FILE_RANDOM,
        CONFIG_TEST_FILE_SHUFFLE
};

constexpr int CONFIG_TEST_FILE_TYPE = CONFIG_TEST_FILE_SHUFFLE;

constexpr uint64_t CONFIG_TEST_FILE_SIZE = 1500_MiB;
