/* ****************************************************************************
* SETTINGS & GLOBALS
* ***************************************************************************/

#pragma once
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
private:
        const bool enabled_;
};


constexpr option depends_on(option opt, option value)
{
        return option(IS_ENABLED(opt) && IS_ENABLED(value));
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

/******************************************************************************
* FILE SECTION
*****************************************************************************/

constexpr const char* CONFIG_INPUT_FILENAME  = "input";
constexpr const char* CONFIG_OUTPUT_FILENAME = "output";
constexpr const char  CONFIG_CHUNK_NAME_SEP = '_';
constexpr const char* CONFIG_CHUNK_DIR = "chunks";

constexpr auto CONFIG_REMOVE_TMP_FILES = config::OFF;

/******************************************************************************
* SORT SECTION
*****************************************************************************/

enum
{
        CONFIG_SORT_HEAP,
        CONFIG_SORT_STD
};

constexpr int CONFIG_SORT_ALGO = CONFIG_SORT_STD;

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
constexpr size_t CONFIG_MEM_AVAIL = 10_MiB;

constexpr float CONFIG_IO_BUFF_RATIO = 0.5f;

/******************************************************************************
* LOG SECTION
*****************************************************************************/

constexpr auto CONFIG_FORCE_DEBUG = config::OFF;

constexpr auto CONFIG_PERF_MEASURE_GET_NEXT_SORT_TASK = config::OFF;

constexpr int CONFIG_INFO_LEVEL = 2;


/******************************************************************************
 * TEST SECTION
 *****************************************************************************/

constexpr auto CONFIG_GENERATE_TEST_FILE = config::OFF;

constexpr auto CONFIG_SKIP_SORT = config::conflicts_with(CONFIG_GENERATE_TEST_FILE, config::ON);

constexpr auto CONFIG_REMOVE_RESULT = config::ON;

constexpr auto CONFIG_CHECK_RESULT = config::ON;

constexpr auto CONFIG_CHECK_HASH = config::ON;

constexpr const char* CONFIG_ORIGIN_HASH_FILENAME = "origin.hash";

constexpr auto CONFIG_PRINT_RESULT = config::OFF;

enum
{
        CONFIG_TEST_FILE_RANDOM,
        CONFIG_TEST_FILE_SHUFFLE
};

constexpr int CONFIG_TEST_FILE_TYPE = CONFIG_TEST_FILE_SHUFFLE;

constexpr uint64_t CONFIG_TEST_FILE_SIZE = 500_MiB;
