// Details for templated Spreadsort-based integer_sort.

//          Copyright Steven J. Ross 2001 - 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org/libs/sort for library home page.

/*
Some improvements suggested by:
Phil Endecott and Frank Gennari
*/

#pragma once

#include <algorithm>
#include <vector>
#include <limits>
#include <functional>
#include <cstdint>

#define BOOST_STATIC_ASSERT( ... ) static_assert(__VA_ARGS__, #__VA_ARGS__)

namespace sort
{
namespace boost {
using uintmax_t = uintptr_t;

template <typename T, typename R = void>
struct enable_if_has_type
{
        typedef R type;
};

template <bool B, class T = void>
struct enable_if_c
{
        typedef T type;
};

template <class T>
struct enable_if_c<false, T>
{
};

template <class Cond, class T = void>
struct enable_if : public enable_if_c<Cond::value, T>
{
};

template <bool B, class T>
struct lazy_enable_if_c
{
        typedef typename T::type type;
};

template <class T>
struct lazy_enable_if_c<false, T>
{
};

template <class Cond, class T>
struct lazy_enable_if : public lazy_enable_if_c<Cond::value, T>
{
};


template <bool B, class T = void>
struct disable_if_c
{
        typedef T type;
};

template <class T>
struct disable_if_c<true, T>
{
};

template <class Cond, class T = void>
struct disable_if : public disable_if_c<Cond::value, T>
{
};

template <bool B, class T>
struct lazy_disable_if_c
{
        typedef typename T::type type;
};

template <class T>
struct lazy_disable_if_c<true, T>
{
};

template <class Cond, class T>
struct lazy_disable_if : public lazy_disable_if_c<Cond::value, T>
{
};
}


namespace detail
{

//Tuning constants
//This should be tuned to your processor cache;
//if you go too large you get cache misses on bins
//The smaller this number, the less worst-case memory usage.
//If too small, too many recursions slow down spreadsort
enum
{
        max_splits = 11,
        //It's better to have a few cache misses and finish sorting
        //than to run another iteration
        max_finishing_splits = max_splits + 1,
        //Sets the minimum number of items per bin.
        int_log_mean_bin_size = 2,
        //Used to force a comparison-based sorting for small bins, if it's faster.
        //Minimum value 1
        int_log_min_split_count = 9,
        //This is the minimum split count to use spreadsort when it will finish in one
        //iteration.  Make this larger the faster std::sort is relative to integer_sort.
        int_log_finishing_count = 31,
        //Sets the minimum number of items per bin for floating point.
        float_log_mean_bin_size = 2,
        //Used to force a comparison-based sorting for small bins, if it's faster.
        //Minimum value 1
        float_log_min_split_count = 8,
        //This is the minimum split count to use spreadsort when it will finish in one
        //iteration.  Make this larger the faster std::sort is relative to float_sort.
        float_log_finishing_count = 4,
        //There is a minimum size below which it is not worth using spreadsort
        min_sort_size = 1000
};


//This only works on unsigned data types
template <typename T>
inline unsigned
rough_log_2_size(const T& input)
{
        unsigned result = 0;
        //The && is necessary on some compilers to avoid infinite loops
        //it doesn't significantly impair performance
        while ((input >> result) && (result < (8 * sizeof(T)))) ++result;
        return result;
}

//Gets the minimum size to call spreadsort on to control worst-case runtime.
//This is called for a set of bins, instead of bin-by-bin, to minimize
//runtime overhead.
//This could be replaced by a lookup table of sizeof(Div_type)*8 but this
//function is more general.
template <unsigned log_mean_bin_size,
        unsigned log_min_split_count, unsigned log_finishing_count>
        inline size_t
        get_min_count(unsigned log_range)
{
        const size_t typed_one = 1;
        const unsigned min_size = log_mean_bin_size + log_min_split_count;
        //Assuring that constants have valid settings
        BOOST_STATIC_ASSERT(log_min_split_count <= max_splits &&
                log_min_split_count > 0);
        BOOST_STATIC_ASSERT(max_splits > 1 &&
                max_splits < (8 * sizeof(unsigned)));
        BOOST_STATIC_ASSERT(max_finishing_splits >= max_splits &&
                max_finishing_splits < (8 * sizeof(unsigned)));
        BOOST_STATIC_ASSERT(log_mean_bin_size >= 0);
        BOOST_STATIC_ASSERT(log_finishing_count >= 0);
        //if we can complete in one iteration, do so
        //This first check allows the compiler to optimize never-executed code out
        if (log_finishing_count < min_size)
        {
                if (log_range <= min_size && log_range <= max_splits)
                {
                        //Return no smaller than a certain minimum limit
                        if (log_range <= log_finishing_count)
                                return typed_one << log_finishing_count;
                        return typed_one << log_range;
                }
        }
        const unsigned base_iterations = max_splits - log_min_split_count;
        //sum of n to n + x = ((x + 1) * (n + (n + x)))/2 + log_mean_bin_size
        const unsigned base_range =
                ((base_iterations + 1) * (max_splits + log_min_split_count)) / 2
                + log_mean_bin_size;
        //Calculating the required number of iterations, and returning
        //1 << (iteration_count + min_size)
        if (log_range < base_range)
        {
                unsigned result = log_min_split_count;
                for (unsigned offset = min_size; offset < log_range;
                        offset += ++result);
                //Preventing overflow; this situation shouldn't occur
                if ((result + log_mean_bin_size) >= (8 * sizeof(size_t)))
                        return typed_one << ((8 * sizeof(size_t)) - 1);
                return typed_one << (result + log_mean_bin_size);
        }
        //A quick division can calculate the worst-case runtime for larger ranges
        unsigned remainder = log_range - base_range;
        //the max_splits - 1 is used to calculate the ceiling of the division
        unsigned bit_length = ((((max_splits - 1) + remainder) / max_splits)
                + base_iterations + min_size);
        //Preventing overflow; this situation shouldn't occur
        if (bit_length >= (8 * sizeof(size_t)))
                return typed_one << ((8 * sizeof(size_t)) - 1);
        //n(log_range)/max_splits + C, optimizing worst-case performance
        return typed_one << bit_length;
}

// Resizes the bin cache and bin sizes, and initializes each bin size to 0.
// This generates the memory overhead to use in radix sorting.
template <class RandomAccessIter>
inline RandomAccessIter*
size_bins(size_t* bin_sizes, std::vector<RandomAccessIter>
        & bin_cache, unsigned cache_offset, unsigned& cache_end,
        unsigned bin_count)
{
        // Clear the bin sizes
        for (size_t u = 0; u < bin_count; u++)
                bin_sizes[u] = 0;
        //Make sure there is space for the bins
        cache_end = cache_offset + bin_count;
        if (cache_end > bin_cache.size())
                bin_cache.resize(cache_end);
        return &(bin_cache[cache_offset]);
}

// Return true if the list is sorted.  Otherwise, find the minimum and
// maximum using <.
template <class RandomAccessIter>
inline bool
is_sorted_or_find_extremes(RandomAccessIter current, RandomAccessIter last,
        RandomAccessIter& max, RandomAccessIter& min)
{
        min = max = current;
        //This assumes we have more than 1 element based on prior checks.
        while (!(*(current + 1) < *current))
        {
                //If everything is in sorted order, return
                if (++current == last - 1)
                        return true;
        }

        //The maximum is the last sorted element
        max = current;
        //Start from the first unsorted element
        while (++current < last)
        {
                if (*max < *current)
                        max = current;
                else if (*current < *min)
                        min = current;
        }
        return false;
}

// Return true if the list is sorted.  Otherwise, find the minimum and
// maximum.
// Use a user-defined comparison operator
template <class RandomAccessIter, class Compare>
inline bool
is_sorted_or_find_extremes(RandomAccessIter current, RandomAccessIter last,
        RandomAccessIter& max, RandomAccessIter& min,
        Compare comp)
{
        min = max = current;
        while (!comp(*(current + 1), *current))
        {
                //If everything is in sorted order, return
                if (++current == last - 1)
                        return true;
        }

        //The maximum is the last sorted element
        max = current;
        while (++current < last)
        {
                if (comp(*max, *current))
                        max = current;
                else if (comp(*current, *min))
                        min = current;
        }
        return false;
}

//Gets a non-negative right bit shift to operate as a logarithmic divisor
template <unsigned log_mean_bin_size>
inline int
get_log_divisor(size_t count, int log_range)
{
        int log_divisor;
        //If we can finish in one iteration without exceeding either
        //(2 to the max_finishing_splits) or n bins, do so
        if ((log_divisor = log_range - rough_log_2_size(count)) <= 0 &&
                log_range <= max_finishing_splits)
                log_divisor = 0;
        else
        {
                //otherwise divide the data into an optimized number of pieces
                log_divisor += log_mean_bin_size;
                //Cannot exceed max_splits or cache misses slow down bin lookups
                if ((log_range - log_divisor) > max_splits)
                        log_divisor = log_range - max_splits;
        }
        return log_divisor;
}

//Implementation for recursive integer sorting
template <class RandomAccessIter, class Div_type, class Size_type>
inline void
spreadsort_rec(RandomAccessIter first, RandomAccessIter last,
        std::vector<RandomAccessIter>& bin_cache, unsigned cache_offset
        , size_t* bin_sizes)
{
        //This step is roughly 10% of runtime, but it helps avoid worst-case
        //behavior and improve behavior with real data
        //If you know the maximum and minimum ahead of time, you can pass those
        //values in and skip this step for the first iteration
        RandomAccessIter max, min;
        if (is_sorted_or_find_extremes(first, last, max, min))
                return;
        RandomAccessIter* target_bin;
        unsigned log_divisor = get_log_divisor<int_log_mean_bin_size>(
                last - first,
                rough_log_2_size(Size_type((*max >> 0) - (*min >> 0))));
        Div_type div_min = *min >> log_divisor;
        Div_type div_max = *max >> log_divisor;
        unsigned bin_count = unsigned(div_max - div_min) + 1;
        unsigned cache_end;
        RandomAccessIter* bins =
                size_bins(bin_sizes, bin_cache, cache_offset, cache_end,
                        bin_count);

        //Calculating the size of each bin; this takes roughly 10% of runtime
        for (RandomAccessIter current = first; current != last;)
                bin_sizes[size_t((*(current++) >> log_divisor) - div_min)]++;
        //Assign the bin positions
        bins[0] = first;
        for (unsigned u = 0; u < bin_count - 1; u++)
                bins[u + 1] = bins[u] + bin_sizes[u];

        RandomAccessIter nextbinstart = first;
        //Swap into place
        //This dominates runtime, mostly in the swap and bin lookups
        for (unsigned u = 0; u < bin_count - 1; ++u)
        {
                RandomAccessIter* local_bin = bins + u;
                nextbinstart += bin_sizes[u];
                //Iterating over each element in this bin
                for (RandomAccessIter current = *local_bin; current <
                        nextbinstart;
                        ++current)
                {
                        //Swapping elements in current into place until the correct
                        //element has been swapped in
                        for (target_bin = (bins + ((*current >> log_divisor) -
                                div_min));
                                target_bin != local_bin;
                                target_bin = bins + ((*current >> log_divisor) -
                                        div_min))
                        {
                                //3-way swap; this is about 1% faster than a 2-way swap
                                //The main advantage is less copies are involved per item
                                //put in the correct place
                                typename std::iterator_traits<RandomAccessIter>
                                        ::value_type tmp;
                                RandomAccessIter b = (*target_bin)++;
                                RandomAccessIter* b_bin = bins + ((*b >>
                                        log_divisor) - div_min);
                                if (b_bin != local_bin)
                                {
                                        RandomAccessIter c = (*b_bin)++;
                                        tmp = *c;
                                        *c = *b;
                                }
                                else
                                        tmp = *b;
                                *b = *current;
                                *current = tmp;
                        }
                }
                *local_bin = nextbinstart;
        }
        bins[bin_count - 1] = last;

        //If we've bucketsorted, the array is sorted and we should skip recursion
        if (!log_divisor)
                return;
        //log_divisor is the remaining range; calculating the comparison threshold
        size_t max_count =
                get_min_count<int_log_mean_bin_size, int_log_min_split_count,
                int_log_finishing_count>(log_divisor);

        //Recursing
        RandomAccessIter lastPos = first;
        for (unsigned u = cache_offset; u < cache_end; lastPos = bin_cache[u],
                ++u)
        {
                Size_type count = bin_cache[u] - lastPos;
                //don't sort unless there are at least two items to Compare
                if (count < 2)
                        continue;
                //using std::sort if its worst-case is better
                if (count < max_count)
                        std::sort(lastPos, bin_cache[u]);
                else
                        spreadsort_rec<RandomAccessIter, Div_type, Size_type>(
                                lastPos,
                                bin_cache[u],
                                bin_cache,
                                cache_end,
                                bin_sizes);
        }
}

//Generic bitshift-based 3-way swapping code
template <class RandomAccessIter, class Div_type, class Right_shift>
inline void inner_swap_loop(RandomAccessIter* bins,
        const RandomAccessIter& next_bin_start, unsigned ii,
        Right_shift& rshift
        , const unsigned log_divisor,
        const Div_type div_min)
{
        RandomAccessIter* local_bin = bins + ii;
        for (RandomAccessIter current = *local_bin; current < next_bin_start;
                ++current)
        {
                for (RandomAccessIter* target_bin =
                        (bins + (rshift(*current, log_divisor) - div_min));
                        target_bin != local_bin;
                        target_bin = bins + (rshift(*current, log_divisor) -
                                div_min))
                {
                        typename std::iterator_traits<RandomAccessIter>::
                                value_type tmp;
                        RandomAccessIter b = (*target_bin)++;
                        RandomAccessIter* b_bin =
                                bins + (rshift(*b, log_divisor) - div_min);
                        //Three-way swap; if the item to be swapped doesn't belong
                        //in the current bin, swap it to where it belongs
                        if (b_bin != local_bin)
                        {
                                RandomAccessIter c = (*b_bin)++;
                                tmp = *c;
                                *c = *b;
                        }
                        //Note: we could increment current once the swap is done in this case
                        //but that seems to impair performance
                        else
                                tmp = *b;
                        *b = *current;
                        *current = tmp;
                }
        }
        *local_bin = next_bin_start;
}

//Standard swapping wrapper for ascending values
template <class RandomAccessIter, class Div_type, class Right_shift>
inline void swap_loop(RandomAccessIter* bins,
        RandomAccessIter& next_bin_start, unsigned ii,
        Right_shift& rshift
        , const size_t* bin_sizes
        , const unsigned log_divisor, const Div_type div_min)
{
        next_bin_start += bin_sizes[ii];
        inner_swap_loop<RandomAccessIter, Div_type, Right_shift>(bins,
                next_bin_start,
                ii, rshift,
                log_divisor,
                div_min);
}

//Functor implementation for recursive sorting
template <class RandomAccessIter, class Div_type, class Right_shift,
        class Compare, class Size_type, unsigned log_mean_bin_size,
        unsigned log_min_split_count, unsigned log_finishing_count>
        inline void
        spreadsort_rec(RandomAccessIter first, RandomAccessIter last,
                std::vector<RandomAccessIter>& bin_cache, unsigned cache_offset
                , size_t* bin_sizes, Right_shift rshift, Compare comp)
{
        RandomAccessIter max, min;
        if (is_sorted_or_find_extremes(first, last, max, min, comp))
                return;
        unsigned log_divisor = get_log_divisor<log_mean_bin_size>(last - first,
                rough_log_2_size(
                        Size_type(
                                rshift(
                                        *max,
                                        0)
                                - rshift(
                                        *min,
                                        0))));
        Div_type div_min = rshift(*min, log_divisor);
        Div_type div_max = rshift(*max, log_divisor);
        unsigned bin_count = unsigned(div_max - div_min) + 1;
        unsigned cache_end;
        RandomAccessIter* bins = size_bins(bin_sizes, bin_cache, cache_offset,
                cache_end, bin_count);

        //Calculating the size of each bin
        for (RandomAccessIter current = first; current != last;)
                bin_sizes[unsigned(rshift(*(current++), log_divisor) - div_min)]
                ++;
        bins[0] = first;
        for (unsigned u = 0; u < bin_count - 1; u++)
                bins[u + 1] = bins[u] + bin_sizes[u];

        //Swap into place
        RandomAccessIter next_bin_start = first;
        for (unsigned u = 0; u < bin_count - 1; ++u)
                swap_loop<RandomAccessIter, Div_type, Right_shift>(
                        bins, next_bin_start,
                        u, rshift, bin_sizes, log_divisor, div_min);
        bins[bin_count - 1] = last;

        //If we've bucketsorted, the array is sorted
        if (!log_divisor)
                return;

        //Recursing
        size_t max_count = get_min_count<log_mean_bin_size, log_min_split_count,
                log_finishing_count>(log_divisor);
        RandomAccessIter lastPos = first;
        for (unsigned u = cache_offset; u < cache_end; lastPos = bin_cache[u],
                ++u)
        {
                size_t count = bin_cache[u] - lastPos;
                if (count < 2)
                        continue;
                if (count < max_count)
                        std::sort(lastPos, bin_cache[u], comp);
                else
                        spreadsort_rec<
                        RandomAccessIter, Div_type, Right_shift,
                        Compare,
                        Size_type, log_mean_bin_size,
                        log_min_split_count, log_finishing_count
                        >
                        (lastPos, bin_cache[u], bin_cache, cache_end,
                                bin_sizes, rshift, comp);
        }
}

//Functor implementation for recursive sorting with only Shift overridden
template <class RandomAccessIter, class Div_type, class Right_shift,
        class Size_type, unsigned log_mean_bin_size,
        unsigned log_min_split_count, unsigned log_finishing_count>
        inline void
        spreadsort_rec(RandomAccessIter first, RandomAccessIter last,
                std::vector<RandomAccessIter>& bin_cache, unsigned cache_offset
                , size_t* bin_sizes, Right_shift rshift)
{
        RandomAccessIter max, min;
        if (is_sorted_or_find_extremes(first, last, max, min))
                return;
        unsigned log_divisor = get_log_divisor<log_mean_bin_size>(last - first,
                rough_log_2_size(
                        Size_type(
                                rshift(
                                        *max,
                                        0)
                                - rshift(
                                        *min,
                                        0))));
        Div_type div_min = rshift(*min, log_divisor);
        Div_type div_max = rshift(*max, log_divisor);
        unsigned bin_count = unsigned(div_max - div_min) + 1;
        unsigned cache_end;
        RandomAccessIter* bins = size_bins(bin_sizes, bin_cache, cache_offset,
                cache_end, bin_count);

        //Calculating the size of each bin
        for (RandomAccessIter current = first; current != last;)
                bin_sizes[unsigned(rshift(*(current++), log_divisor) - div_min)]
                ++;
        bins[0] = first;
        for (unsigned u = 0; u < bin_count - 1; u++)
                bins[u + 1] = bins[u] + bin_sizes[u];

        //Swap into place
        RandomAccessIter nextbinstart = first;
        for (unsigned ii = 0; ii < bin_count - 1; ++ii)
                swap_loop<RandomAccessIter, Div_type, Right_shift>(
                        bins, nextbinstart,
                        ii, rshift, bin_sizes, log_divisor, div_min);
        bins[bin_count - 1] = last;

        //If we've bucketsorted, the array is sorted
        if (!log_divisor)
                return;

        //Recursing
        size_t max_count = get_min_count<log_mean_bin_size, log_min_split_count,
                log_finishing_count>(log_divisor);
        RandomAccessIter lastPos = first;
        for (unsigned u = cache_offset; u < cache_end; lastPos = bin_cache[u],
                ++u)
        {
                size_t count = bin_cache[u] - lastPos;
                if (count < 2)
                        continue;
                if (count < max_count)
                        std::sort(lastPos, bin_cache[u]);
                else
                        spreadsort_rec<
                        RandomAccessIter, Div_type, Right_shift,
                        Size_type,
                        log_mean_bin_size, log_min_split_count,
                        log_finishing_count>(lastPos,
                                bin_cache[u], bin_cache,
                                cache_end, bin_sizes,
                                rshift);
        }
}

//Holds the bin vector and makes the initial recursive call
template <class RandomAccessIter, class Div_type>
//Only use spreadsort if the integer can fit in a size_t
inline typename boost::enable_if_c<sizeof(Div_type) <= sizeof(size_t),
        void>::type
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type)
{
        size_t bin_sizes[1 << max_finishing_splits];
        std::vector<RandomAccessIter> bin_cache;
        spreadsort_rec<RandomAccessIter, Div_type, size_t>(first, last,
                bin_cache, 0,
                bin_sizes);
}

//Holds the bin vector and makes the initial recursive call
template <class RandomAccessIter, class Div_type>
//Only use spreadsort if the integer can fit in a uintmax_t
inline typename boost::enable_if_c<(sizeof(Div_type) > sizeof(size_t))
&& sizeof(Div_type) <= sizeof(boost::
        uintmax_t), void>::type
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type)
{
        size_t bin_sizes[1 << max_finishing_splits];
        std::vector<RandomAccessIter> bin_cache;
        spreadsort_rec<RandomAccessIter, Div_type, boost::uintmax_t>(first,
                last,
                bin_cache,
                0,
                bin_sizes);
}

template <class RandomAccessIter, class Div_type>
inline typename boost::disable_if_c<sizeof(Div_type) <= sizeof(size_t)
        || sizeof(Div_type) <= sizeof(boost::
                uintmax_t), void>::type
        //defaulting to std::sort when integer_sort won't work
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type)
{
        //Warning that we're using std::sort, even though integer_sort was called
        BOOST_STATIC_ASSERT(sizeof(Div_type) <= sizeof(size_t));
        std::sort(first, last);
}


//Same for the full functor version
template <class RandomAccessIter, class Div_type, class Right_shift,
        class Compare>
        //Only use spreadsort if the integer can fit in a size_t
        inline typename boost::enable_if_c<sizeof(Div_type) <= sizeof(size_t),
        void>::type
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type,
                Right_shift shift, Compare comp)
{
        size_t bin_sizes[1 << max_finishing_splits];
        std::vector<RandomAccessIter> bin_cache;
        spreadsort_rec<RandomAccessIter, Div_type, Right_shift, Compare,
                size_t, int_log_mean_bin_size, int_log_min_split_count,
                int_log_finishing_count>
                (first, last, bin_cache, 0, bin_sizes, shift, comp);
}

template <class RandomAccessIter, class Div_type, class Right_shift,
        class Compare>
        //Only use spreadsort if the integer can fit in a uintmax_t
        inline typename boost::enable_if_c<(sizeof(Div_type) > sizeof(size_t))
        && sizeof(Div_type) <= sizeof(boost::
                uintmax_t), void>::type
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type,
                Right_shift shift, Compare comp)
{
        size_t bin_sizes[1 << max_finishing_splits];
        std::vector<RandomAccessIter> bin_cache;
        spreadsort_rec<RandomAccessIter, Div_type, Right_shift, Compare,
                boost::uintmax_t, int_log_mean_bin_size,
                int_log_min_split_count, int_log_finishing_count>
                (first, last, bin_cache, 0, bin_sizes, shift, comp);
}

template <class RandomAccessIter, class Div_type, class Right_shift,
        class Compare>
        inline typename boost::disable_if_c<sizeof(Div_type) <= sizeof(size_t)
        || sizeof(Div_type) <= sizeof(boost::
                uintmax_t), void>::type
        //defaulting to std::sort when integer_sort won't work
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type,
                Right_shift shift, Compare comp)
{
        //Warning that we're using std::sort, even though integer_sort was called
        BOOST_STATIC_ASSERT(sizeof(Div_type) <= sizeof(size_t));
        std::sort(first, last, comp);
}


//Same for the right shift version
template <class RandomAccessIter, class Div_type, class Right_shift>
//Only use spreadsort if the integer can fit in a size_t
inline typename boost::enable_if_c<sizeof(Div_type) <= sizeof(size_t),
        void>::type
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type,
                Right_shift shift)
{
        size_t bin_sizes[1 << max_finishing_splits];
        std::vector<RandomAccessIter> bin_cache;
        spreadsort_rec<RandomAccessIter, Div_type, Right_shift, size_t,
                int_log_mean_bin_size, int_log_min_split_count,
                int_log_finishing_count>
                (first, last, bin_cache, 0, bin_sizes, shift);
}

template <class RandomAccessIter, class Div_type, class Right_shift>
//Only use spreadsort if the integer can fit in a uintmax_t
inline typename boost::enable_if_c<(sizeof(Div_type) > sizeof(size_t))
&& sizeof(Div_type) <= sizeof(boost::
        uintmax_t), void>::type
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type,
                Right_shift shift)
{
        size_t bin_sizes[1 << max_finishing_splits];
        std::vector<RandomAccessIter> bin_cache;
        spreadsort_rec<RandomAccessIter, Div_type, Right_shift,
                boost::uintmax_t, int_log_mean_bin_size,
                int_log_min_split_count, int_log_finishing_count>
                (first, last, bin_cache, 0, bin_sizes, shift);
}

template <class RandomAccessIter, class Div_type, class Right_shift>
inline typename boost::disable_if_c<sizeof(Div_type) <= sizeof(size_t)
        || sizeof(Div_type) <= sizeof(boost::
                uintmax_t), void>::type
        //defaulting to std::sort when integer_sort won't work
        integer_sort(RandomAccessIter first, RandomAccessIter last, Div_type,
                Right_shift shift)
{
        //Warning that we're using std::sort, even though integer_sort was called
        BOOST_STATIC_ASSERT(sizeof(Div_type) <= sizeof(size_t));
        std::sort(first, last);
}

}

//Top-level sorting call for integers.


/*! \brief Integer sort algorithm using random access iterators.
(All variants fall back to @c std::sort if the data size is too small, < @c detail::min_sort_size).
\details @c integer_sort is a fast templated in-place hybrid radix/comparison algorithm,
which in testing tends to be roughly 50% to 2X faster than @c std::sort for large tests (>=100kB).\n
Worst-case performance is <em>  O(N * (lg(range)/s + s)) </em>,
so @c integer_sort is asymptotically faster
than pure comparison-based algorithms. @c s is @c max_splits, which defaults to 11,
so its worst-case with default settings for 32-bit integers is
<em> O(N * ((32/11) </em> slow radix-based iterations fast comparison-based iterations).\n\n
Some performance plots of runtime vs. n and log(range) are provided:\n
<a href="../../doc/graph/windows_integer_sort.htm"> windows_integer_sort</a>
\n
<a href="../../doc/graph/osx_integer_sort.htm"> osx_integer_sort</a>
\param[in] first Iterator pointer to first element.
\param[in] last Iterator pointing to one beyond the end of data.
\pre [@c first, @c last) is a valid range.
\pre @c RandomAccessIter @c value_type is mutable.
\pre @c RandomAccessIter @c value_type is <a href="http://en.cppreference.com/w/cpp/concept/LessThanComparable">LessThanComparable</a>
\pre @c RandomAccessIter @c value_type supports the @c operator>>,
which returns an integer-type right-shifted a specified number of bits.
\post The elements in the range [@c first, @c last) are sorted in ascending order.
\throws std::exception Propagates exceptions if any of the element comparisons, the element swaps (or moves),
the right shift, subtraction of right-shifted elements, functors, or any operations on iterators throw.
\warning Throwing an exception may cause data loss. This will also throw if a small vector resize throws, in which case there will be no data loss.
\warning Invalid arguments cause undefined behaviour.
\note @c spreadsort function provides a wrapper that calls the fastest sorting algorithm available for a data type,
enabling faster generic-programming.
\remark The lesser of <em> O(N*log(N)) </em> comparisons and <em> O(N*log(K/S + S)) </em>operations worst-case, where:
\remark  *  N is @c last - @c first,
\remark  *  K is the log of the range in bits (32 for 32-bit integers using their full range),
\remark  *  S is a constant called max_splits, defaulting to 11 (except for strings where it is the log of the character size).
*/
template <class RandomAccessIter>
inline void integer_sort(RandomAccessIter first, RandomAccessIter last)
{
        // Don't sort if it's too small to optimize.
        if (last - first < detail::min_sort_size)
                std::sort(first, last);
        else
                detail::integer_sort(first, last, *first >> 0);
}

/*! \brief Integer sort algorithm using random access iterators with both right-shift and user-defined comparison operator.
(All variants fall back to @c std::sort if the data size is too small, < @c detail::min_sort_size).
\details @c integer_sort is a fast templated in-place hybrid radix/comparison algorithm,
which in testing tends to be roughly 50% to 2X faster than @c std::sort for large tests (>=100kB).\n
Worst-case performance is <em>  O(N * (lg(range)/s + s)) </em>,
so @c integer_sort is asymptotically faster
than pure comparison-based algorithms. @c s is @c max_splits, which defaults to 11,
so its worst-case with default settings for 32-bit integers is
<em> O(N * ((32/11) </em> slow radix-based iterations fast comparison-based iterations).\n\n
Some performance plots of runtime vs. n and log(range) are provided:\n
<a href="../../doc/graph/windows_integer_sort.htm"> windows_integer_sort</a>
\n
<a href="../../doc/graph/osx_integer_sort.htm"> osx_integer_sort</a>
\param[in] first Iterator pointer to first element.
\param[in] last Iterator pointing to one beyond the end of data.
\param[in] shift Functor that returns the result of shifting the value_type right a specified number of bits.
\param[in] comp A binary functor that returns whether the first element passed to it should go before the second in order.
\pre [@c first, @c last) is a valid range.
\pre @c RandomAccessIter @c value_type is mutable.
\post The elements in the range [@c first, @c last) are sorted in ascending order.
\return @c void.
\throws std::exception Propagates exceptions if any of the element comparisons, the element swaps (or moves),
the right shift, subtraction of right-shifted elements, functors,
or any operations on iterators throw.
\warning Throwing an exception may cause data loss. This will also throw if a small vector resize throws, in which case there will be no data loss.
\warning Invalid arguments cause undefined behaviour.
\note @c spreadsort function provides a wrapper that calls the fastest sorting algorithm available for a data type,
enabling faster generic-programming.
\remark The lesser of <em> O(N*log(N)) </em> comparisons and <em> O(N*log(K/S + S)) </em>operations worst-case, where:
\remark  *  N is @c last - @c first,
\remark  *  K is the log of the range in bits (32 for 32-bit integers using their full range),
\remark  *  S is a constant called max_splits, defaulting to 11 (except for strings where it is the log of the character size).
*/
template <class RandomAccessIter, class Right_shift, class Compare>
inline void integer_sort(RandomAccessIter first, RandomAccessIter last,
        Right_shift shift, Compare comp) {
        if (last - first < detail::min_sort_size)
                std::sort(first, last, comp);
        else
                detail::integer_sort(first, last, shift(*first, 0), shift, comp);
}

/*! \brief Integer sort algorithm using random access iterators with just right-shift functor.
(All variants fall back to @c std::sort if the data size is too small, < @c detail::min_sort_size).
\details @c integer_sort is a fast templated in-place hybrid radix/comparison algorithm,
which in testing tends to be roughly 50% to 2X faster than @c std::sort for large tests (>=100kB).\n
\par Performance:
Worst-case performance is <em>  O(N * (lg(range)/s + s)) </em>,
so @c integer_sort is asymptotically faster
than pure comparison-based algorithms. @c s is @c max_splits, which defaults to 11,
so its worst-case with default settings for 32-bit integers is
<em> O(N * ((32/11) </em> slow radix-based iterations fast comparison-based iterations).\n\n
Some performance plots of runtime vs. n and log(range) are provided:\n
* <a href="../../doc/graph/windows_integer_sort.htm"> windows_integer_sort</a>\n
* <a href="../../doc/graph/osx_integer_sort.htm"> osx_integer_sort</a>
\param[in] first Iterator pointer to first element.
\param[in] last Iterator pointing to one beyond the end of data.
\param[in] shift A functor that returns the result of shifting the value_type right a specified number of bits.
\pre [@c first, @c last) is a valid range.
\pre @c RandomAccessIter @c value_type is mutable.
\pre @c RandomAccessIter @c value_type is <a href="http://en.cppreference.com/w/cpp/concept/LessThanComparable">LessThanComparable</a>
\post The elements in the range [@c first, @c last) are sorted in ascending order.
\throws std::exception Propagates exceptions if any of the element comparisons, the element swaps (or moves),
the right shift, subtraction of right-shifted elements, functors,
or any operations on iterators throw.
\warning Throwing an exception may cause data loss. This will also throw if a small vector resize throws, in which case there will be no data loss.
\warning Invalid arguments cause undefined behaviour.
\note @c spreadsort function provides a wrapper that calls the fastest sorting algorithm available for a data type,
enabling faster generic-programming.
\remark The lesser of <em> O(N*log(N)) </em> comparisons and <em> O(N*log(K/S + S)) </em>operations worst-case, where:
\remark  *  N is @c last - @c first,
\remark  *  K is the log of the range in bits (32 for 32-bit integers using their full range),
\remark  *  S is a constant called max_splits, defaulting to 11 (except for strings where it is the log of the character size).
*/
template <class RandomAccessIter, class Right_shift>
inline void integer_sort(RandomAccessIter first, RandomAccessIter last,
        Right_shift shift) {
        if (last - first < detail::min_sort_size)
                std::sort(first, last);
        else
                detail::integer_sort(first, last, shift(*first, 0), shift);
}


}
