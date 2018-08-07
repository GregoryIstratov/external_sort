/* ****************************************************************************
* SETTINGS & GLOBALS
* ***************************************************************************/

#pragma once
#include <cstddef>
#include "tools/literals.hpp"

class config_option
{
public:
        explicit 
        constexpr config_option(bool enabled) 
        : enabled_(enabled) {}

        friend static constexpr bool IS_ENABLED(config_option option)
        {
                return option.enabled_;
        }
private:
        const bool enabled_;
};

static constexpr config_option Enabled(true);
static constexpr config_option Disabled(false);

/******************************************************************************
* COMMON SECTION
*****************************************************************************/

using CONFIG_DATA_TYPE = uint32_t;

/* Number of threads if hardware_concurrency() fails */
static constexpr size_t CONFIG_DEFAULT_THREAD_NUMBER = 2;

/******************************************************************************
* FILE SECTION
*****************************************************************************/

static constexpr const char* CONFIG_INPUT_FILENAME  = "input";
static constexpr const char* CONFIG_OUTPUT_FILENAME = "output";
static constexpr const char  CONFIG_CHUNK_NAME_SEP = '_';
static constexpr const char* CONFIG_CHUNK_DIR = "chunks";

static constexpr config_option CONFIG_REMOVE_TMP_FILES = Enabled;

/******************************************************************************
* SORT SECTION
*****************************************************************************/

enum
{
        CONFIG_SORT_HEAP,
        CONFIG_SORT_STD
};

static constexpr int CONFIG_SORT_ALGO = CONFIG_SORT_STD;

/******************************************************************************
* MERGE SECTION
*****************************************************************************/

static constexpr auto CONFIG_N_WAY_FLAT = Enabled;

/* 0 - auto, n > 2 = n */
static constexpr int CONFIG_N_WAY_MERGE_N = 0;

static constexpr int CONFIG_TREE_HEIGH = 2;

/******************************************************************************
* MEMORY SECTION
*****************************************************************************/

/* Default page size on most systems */
static constexpr size_t PAGE_SIZE = 4096;

/* -x MiB for program itself and some part of each thread stack*/
static constexpr size_t CONFIG_MEM_AVAIL = 500_KiB;

static constexpr float CONFIG_IO_BUFF_RATIO = 0.5f;

/******************************************************************************
* LOG SECTION
*****************************************************************************/

static constexpr auto CONFIG_FORCE_DEBUG = Disabled;

static constexpr auto CONFIG_PERF_MEASURE_GET_NEXT_SORT_TASK = Disabled;

static constexpr int CONFIG_INFO_LEVEL = 2;


/******************************************************************************
 * TEST SECTION
 *****************************************************************************/

static constexpr auto CONFIG_SKIP_SORT = Disabled;

static constexpr auto CONFIG_REMOVE_RESULT = Enabled;

static constexpr auto CONFIG_CHECK_RESULT = Enabled;

static constexpr auto CONFIG_CHECK_HASH = Enabled;

static constexpr const char* CONFIG_ORIGIN_HASH_FILENAME = "origin.hash";

static constexpr auto CONFIG_PRINT_RESULT = Disabled;

static constexpr auto CONFIG_GENERATE_TEST_FILE = Enabled;

enum
{
        CONFIG_TEST_FILE_RANDOM,
        CONFIG_TEST_FILE_SHUFFLE
};

static constexpr int CONFIG_TEST_FILE_TYPE = CONFIG_TEST_FILE_SHUFFLE;

static constexpr uint64_t CONFIG_TEST_FILE_SIZE = 5_MiB;
