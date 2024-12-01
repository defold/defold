#pragma once

#include "partition_branchless.h"

namespace ppqsort::impl {
    /**
     * Partition [__first, __last) using the comp
     * Assumes that *begin has the chosen pivot
     * Assumes that input range has at least 3 elements
     * @tparam RandomIt
     * @tparam Compare
     * @tparam T
     * @param begin
     * @param end
     * @param comp
     * @return Iterator to pivot
     */
    template <typename RandomIt, typename Compare, typename T = typename std::iterator_traits<RandomIt>::value_type>
    RandomIt partition_to_left(RandomIt begin, RandomIt end, Compare comp) {
        T pivot = std::move(*begin);
        RandomIt first = begin;
        RandomIt last = end;

        // find first elem greater than pivot from start
        if (comp(pivot, *(end - 1)))
            // guarded
            while (!comp(pivot, *++first));
        else
            while (++first < last && !comp(pivot, *first));

        // find first elem from end, which is less or equal to pivot
        if (first < last) {
            // always guarded, median of three guarantees it
            while (comp(pivot, *--last));
        }

        // iterate from last and first and swap if needed
        while (first < last) {
            std::iter_swap(first, last);
            while (!comp(pivot, *++first));
            while (comp(pivot, *--last));
        }

        // move pivot from start to according place
        RandomIt pivotPos = --first;
        if (begin != pivotPos)
            *begin = std::move(*pivotPos);
        *pivotPos = std::move(pivot);
        return pivotPos;
    }

    template <typename RandomIt, typename Compare, typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline std::pair<RandomIt, bool> partition_to_right(RandomIt begin, RandomIt end, Compare comp) {
        T pivot = std::move(*begin);
        RandomIt first = begin;
        RandomIt last = end;

        // find first elem greater or equal to the pivot from start
        // the median of three guarantees it is guarded
        while (comp(*++first, pivot));

        // find first elem from end, which is less than pivot
        if (begin == first - 1) {
            while (first < last && !comp(*--last, pivot));
        } else {
            while (!comp(*--last, pivot));
        }
        bool already_partitioned = first >= last;


        // iterate from last and first and swap if needed
        while (first < last) {
            std::iter_swap(first, last);
            while (comp(*++first, pivot));
            while (!comp(*--last, pivot));
        }

        // move pivot from start to according place
        RandomIt pivot_pos = --first;
        if (begin != pivot_pos)
            *begin = std::move(*pivot_pos);
        *pivot_pos = std::move(pivot);
        return std::make_pair(pivot_pos, already_partitioned);
    }

    template <bool branchless, typename RandomIt, typename Compare,
        typename T = typename std::iterator_traits<RandomIt>::value_type,
        typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline std::pair<RandomIt, bool> seq_cleanup(const RandomIt& g_begin, const T& pivot,
                                                 const Compare& comp, const diff_t& g_first_offset,
                                                 const diff_t& g_last_offset, bool g_already_partitioned) {


        RandomIt final_first = g_begin + g_first_offset - 1;
        RandomIt final_last = g_begin + g_last_offset + 1;

        while (final_first < final_last && comp(*++final_first, pivot));
        while (final_first < final_last && !comp(*--final_last, pivot));
        const bool cleanup_already_partitioned = final_first >= final_last;

        if constexpr (branchless) {
            if (!cleanup_already_partitioned)
                partition_branchless_core(final_first, final_last, pivot, comp);
        } else {
            // iterate from last and first and swap if needed
            while (final_first < final_last) {
                // this swap guarantees, that we do not have to check for bounds
                std::iter_swap(final_first, final_last);
                while (comp(*++final_first, pivot));
                while (!comp(*--final_last, pivot));
            }
        }

        g_already_partitioned &= cleanup_already_partitioned;

        // move pivot from start to according place
        RandomIt pivot_pos = --final_first;
        if (g_begin != pivot_pos)
            *g_begin = std::move(*pivot_pos);
        *pivot_pos = std::move(pivot);
        return std::make_pair(pivot_pos, g_already_partitioned);
    }
}
