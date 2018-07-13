#pragma once

#include <cstddef>

/* ****************************************************************************
* SETTINGS & GLOBALS
* ***************************************************************************/
static constexpr uint64_t KILOBYTE = 1024;
static constexpr uint64_t MEGABYTE = 1024 * 1024;
static constexpr uint64_t GIGABYTE = 1024 * MEGABYTE;
static constexpr uint64_t TERABYTE = 1024 * GIGABYTE;

/* Number of threads if hardware_concurrency() returns 0 */
static constexpr size_t CONFIG_DEFAULT_THREAD_NUMBER = 2;

static constexpr const char* CONFIG_INPUT_FILENAME  = "input";
static constexpr const char* CONFIG_OUTPUT_FILENAME = "output";
static constexpr const char* CONFIG_SCHUNK_FILENAME_PAT = "c";

/* 0 - auto, n > 2 = n */
#define CONFIG_N_WAY_MERGE_N 0

#if CONFIG_N_WAY_MERGE_N == 0
static constexpr int CONFIG_TREE_HEIGH = 2;
#endif

/* -5 MiB for program itself and some part of each thread stack*/
static constexpr size_t CONFIG_MEM_AVAIL = 128 * MEGABYTE;

static constexpr float CONFIG_IO_BUFF_RATIO = 0.5f;

using CONFIG_DATA_TYPE = uint32_t;

/* Removes temporary results. */
#define CONFIG_REMOVE_TMP_FILES 1

#define CONFIG_INFO_LEVEL 2

enum
{
        CONFIG_SORT_HEAP,
        CONFIG_SORT_STD
};

static constexpr int CONFIG_SORT_ALGO = CONFIG_SORT_STD;

#define CONFIG_CHECK_RESULT 0
