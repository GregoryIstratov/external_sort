/* ****************************************************************************
* SETTINGS & GLOBALS
* ***************************************************************************/

#pragma once
#include <cstddef>
#include "tools/literals.hpp"

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

#define CONFIG_REMOVE_TMP_FILES 0

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

#define CONFIG_N_WAY_FLAT 1

/* 0 - auto, n > 2 = n */
#define CONFIG_N_WAY_MERGE_N 2

static constexpr int CONFIG_TREE_HEIGH = 2;

/******************************************************************************
* MEMORY SECTION
*****************************************************************************/

/* Default page size on most systems */
static constexpr size_t PAGE_SIZE = 4096;

/* -x MiB for program itself and some part of each thread stack*/
static constexpr size_t CONFIG_MEM_AVAIL = 10_MiB;

static constexpr float CONFIG_IO_BUFF_RATIO = 0.5f;

/******************************************************************************
* LOG SECTION
*****************************************************************************/

#define CONFIG_FORCE_DEBUG 0

#define CONFIG_PERF_MEASURE_GET_NEXT_SORT_TASK 0

#define CONFIG_INFO_LEVEL 2


/******************************************************************************
 * TEST SECTION
 *****************************************************************************/

#define CONFIG_SKIP_SORT 1

#define CONFIG_REMOVE_RESULT 1

#define CONFIG_CHECK_RESULT 1

#define CONFIG_CHECK_HASH 1

static constexpr const char* CONFIG_ORIGIN_HASH_FILENAME = "origin.hash";

#define CONFIG_PRINT_RESULT 0

#define CONFIG_GENERATE_TEST_FILE 0

enum
{
        CONFIG_TEST_FILE_RANDOM,
        CONFIG_TEST_FILE_SHUFFLE
};

static constexpr int CONFIG_TEST_FILE_TYPE = CONFIG_TEST_FILE_SHUFFLE;

static constexpr uint64_t CONFIG_TEST_FILE_SIZE = 500_MiB;
