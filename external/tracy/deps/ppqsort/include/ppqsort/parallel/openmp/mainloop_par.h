#pragma once

#include <omp.h>

#include "../../mainloop.h"
#include "partition_par.h"
#include "partition_branchless_par.h"

namespace ppqsort::impl {

    namespace openmp {
        template <typename RandomIt, typename Compare>
        bool is_sorted_par(RandomIt begin, RandomIt end, Compare comp,
                           const std::size_t size, const int n_threads, bool leftmost) {
            if (n_threads < 2)
                return leftmost ? partial_insertion_sort(begin, end, comp) : partial_insertion_sort_unguarded(begin, end, comp);

            std::vector<uint8_t> sorted(n_threads);
            #pragma omp parallel num_threads(n_threads)
            {
                const int tid = omp_get_thread_num();
                const std::size_t chunk_size = (size + n_threads - 1) / n_threads;
                const std::size_t my_begin = tid * chunk_size;
                const std::size_t my_end = std::min(my_begin + chunk_size + 1, size);
                sorted[tid] = std::is_sorted(begin + my_begin, begin + my_end, comp);
            }
            return std::all_of(sorted.begin(), sorted.end(), [](const uint8_t x) { return x; });
        }


        template <typename RandomIt, typename Compare,
                  bool branchless,
                  typename T = typename std::iterator_traits<RandomIt>::value_type,
                  typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
        inline void par_loop(RandomIt begin, RandomIt end, Compare comp,
                             diff_t bad_allowed, diff_t seq_thr, int threads,
                             bool leftmost = true)
        {
            constexpr int insertion_threshold = branchless ?
                                                parameters::insertion_threshold_primitive
                                                : parameters::insertion_threshold;

            while (true) {
                diff_t size = end - begin;
                if (size < seq_thr)
                    return seq_loop<RandomIt, Compare, branchless>(begin, end, comp, bad_allowed, leftmost);

                choose_pivot<branchless>(begin, end, size, comp);

                // pivot is the same as previous pivot
                // put same elements to the left, and we do not have to recurse
                if (!leftmost && !comp(*(begin-1), *begin)) {
                    begin = partition_to_left(begin, end, comp) + 1;
                    continue;
                }

                std::pair<RandomIt, bool> part_result;
                if (threads < 2) {
                    part_result = branchless ? partition_right_branchless(begin, end, comp)
                                               : partition_to_right(begin, end, comp);
                } else {
                    part_result = branchless ? partition_right_branchless_par(begin, end, comp, threads)
                                               : partition_to_right_par(begin, end, comp, threads);
                }
                RandomIt pivot_pos = part_result.first;
                bool already_partitioned = part_result.second;

                diff_t l_size = pivot_pos - begin;
                diff_t r_size = end - (pivot_pos + 1);

                if (already_partitioned) {
                    bool left = false;
                    bool right = false;
                    if (l_size > insertion_threshold)
                        left = is_sorted_par(begin, pivot_pos, comp, l_size, threads, leftmost);
                    if (r_size > insertion_threshold)
                        right = is_sorted_par(pivot_pos + 1, end, comp, r_size, threads, false);
                    if (left && right) {
                        return;
                    } if (left) {
                        begin = ++pivot_pos;
                        continue;
                    } if (right) {
                        end = pivot_pos;
                        continue;
                    }
                }

                bool highly_unbalanced = l_size < size / parameters::partition_ratio ||
                                         r_size < size / parameters::partition_ratio;

                if (highly_unbalanced) {
                    // switch to heapsort
                    if (--bad_allowed == 0) {
                        std::make_heap(begin, end, comp);
                        std::sort_heap(begin, end, comp);
                        return;
                    }
                    // partition unbalanced, shuffle elements
                    deterministic_shuffle(begin, end, l_size, r_size, pivot_pos, insertion_threshold);
                }

                threads >>= 1;
                #pragma omp task
                par_loop<RandomIt, Compare, branchless>(begin, pivot_pos, comp, bad_allowed, seq_thr,
                                                        threads, leftmost);
                leftmost = false;
                begin = ++pivot_pos;
            }
        }
    }

    template <bool Force_branchless = false,
         typename RandomIt,
         typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>,
         bool Branchless = use_branchless<typename std::iterator_traits<RandomIt>::value_type, Compare>::value>
    void par_ppqsort(RandomIt begin, RandomIt end,
                     Compare comp = Compare(), const int threads = omp_get_max_threads()) {
        if (begin == end)
            return;
        constexpr bool branchless = Force_branchless || Branchless;
        auto size = end - begin;
        if ((threads < 2) || (size < parameters::seq_threshold))
            return seq_loop<RandomIt, Compare, branchless>(begin, end, comp, log2(size));

        int seq_thr = size / threads / parameters::par_thr_div;
        seq_thr = std::max(seq_thr, branchless ? parameters::insertion_threshold_primitive
                                               : parameters::insertion_threshold);
        omp_set_max_active_levels(2);
        #pragma omp parallel
        {
            #pragma omp single
            {
                #pragma omp task
                openmp::par_loop<RandomIt, Compare, branchless>(begin, end, comp,
                                                                log2(end - begin),
                                                                seq_thr, threads);
            }
        }
    }

    template <bool Force_branchless = false,
             typename RandomIt,
             typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>>
        void par_ppqsort(RandomIt begin, RandomIt end, const int threads) {
            Compare comp = Compare();
            par_ppqsort<Force_branchless>(begin, end, comp, threads);
        }
}