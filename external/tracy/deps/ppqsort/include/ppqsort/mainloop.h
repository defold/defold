#pragma once

#include <algorithm>
#include <ranges>

#include "parameters.h"
#include "partition.h"
#include "partition_branchless.h"
#include "sorts.h"

namespace ppqsort::impl {
    template <class T> struct is_default_comparator : std::false_type {};
    template <> struct is_default_comparator<std::less<>> : std::true_type {};
    template <class T> struct is_default_comparator<std::less<T>> : std::true_type {};
    template <class T> struct is_default_comparator<std::greater<T>> : std::true_type {};
    template <> struct is_default_comparator<std::ranges::less> : std::true_type {};
    template <> struct is_default_comparator<std::ranges::greater> : std::true_type {};

    template <typename T, class Compare>
    using use_branchless = std::integral_constant<bool, std::is_arithmetic_v<T> &&
                                                        is_default_comparator<std::decay_t<Compare>>::value>;

    template <typename RandomIt, typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void deterministic_shuffle(RandomIt begin, RandomIt end, const diff_t l_size, const diff_t r_size,
                                       RandomIt pivot_pos, const int insertion_threshold) {
        if (l_size >= insertion_threshold) {
            std::iter_swap(begin,             begin + l_size / 4);
            std::iter_swap(pivot_pos - 1, pivot_pos - l_size / 4);

            if (l_size > parameters::median_threshold) {
                std::iter_swap(begin + 1,         begin + (l_size / 4 + 1));
                std::iter_swap(begin + 2,         begin + (l_size / 4 + 2));
                std::iter_swap(pivot_pos - 2, pivot_pos - (l_size / 4 + 1));
                std::iter_swap(pivot_pos - 3, pivot_pos - (l_size / 4 + 2));
            }
        }

        if (r_size >= insertion_threshold) {
            std::iter_swap(pivot_pos + 1, pivot_pos + (1 + r_size / 4));
            std::iter_swap(end - 1,                   end - r_size / 4);

            if (r_size > parameters::median_threshold) {
                std::iter_swap(pivot_pos + 2, pivot_pos + (2 + r_size / 4));
                std::iter_swap(pivot_pos + 3, pivot_pos + (3 + r_size / 4));
                std::iter_swap(end - 2,             end - (1 + r_size / 4));
                std::iter_swap(end - 3,             end - (2 + r_size / 4));
            }
        }
    }

    template <bool branchless, typename RandomIt, class Compare,
              typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void median_of_three_medians(const RandomIt & begin, const RandomIt & end,
                                        const diff_t & size, Compare comp) {
        RandomIt mid = begin + size/2;
        if constexpr (branchless) {
            sort_3_branchless(begin, mid, end - 1, comp);
            sort_3_branchless(begin + 1, mid + 1, end - 2, comp);
            sort_3_branchless(begin + 2, mid - 1, end - 3, comp);
            sort_3_branchless(mid - 1, mid, mid + 1, comp);
        } else {
            sort_3(begin, mid, end - 1, comp);
            sort_3(begin + 1, mid + 1, end - 2, comp);
            sort_3(begin + 2, mid - 1, end - 3, comp);
            sort_3(mid - 1, mid, mid + 1, comp);
        }
        std::iter_swap(begin, mid);
    }

    template <typename RandomIt, class Compare,
              typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void median_of_five_medians(const RandomIt & begin, const RandomIt & end,
                                       const diff_t & size, Compare comp) {
        RandomIt x2 = begin + size/4;
        RandomIt mid = begin + size/2;
        RandomIt x4 = begin + size/4 * 3;
        sort_5_branchless(begin, x2, mid, x4, end - 1, comp);
        sort_5_branchless(begin + 1, x2 + 1, mid + 1, x4 + 1, end - 2, comp);
        sort_5_branchless(begin + 2, x2 + 2, mid + 2, x4 + 2, end - 3, comp);
        sort_5_branchless(begin + 3, x2 - 1, mid - 1, x4 - 1, end - 4, comp);
        sort_5_branchless(begin + 4, x2 - 2, mid - 2, x4 - 2, end - 5, comp);
        sort_5_branchless(mid - 2, mid - 1, mid, mid + 1, mid + 2, comp);
        std::iter_swap(begin, mid);
    }

    template <bool branchless, typename RandomIt, class Compare,
              typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void choose_pivot(const RandomIt & begin, const RandomIt & end, const diff_t & size, Compare comp) {
        if (size > parameters::median_threshold) {
            median_of_three_medians<branchless>(begin, end, size, comp);
        } else {
            RandomIt mid = begin + size/2;
            if constexpr (branchless)
                sort_3_branchless(mid, begin, end - 1, comp);
            else
                sort_3(mid, begin, end - 1, comp);
        }
    }

    template <typename RandomIt, typename Compare, bool branchless,
            typename T = typename std::iterator_traits<RandomIt>::value_type,
            typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void seq_loop(RandomIt begin, RandomIt end, Compare comp, diff_t bad_allowed, bool leftmost = true) {
        constexpr int insertion_threshold = branchless ?
                                                  parameters::insertion_threshold_primitive
                                                  : parameters::insertion_threshold;

        while (true) {
            diff_t size = end - begin;

            if (size <= insertion_threshold) {
                if (leftmost)
                    insertion_sort(begin, end, comp);
                else
                    insertion_sort_unguarded(begin, end, comp);
                return;
            }

            choose_pivot<branchless>(begin, end, size, comp);

            // pivot is the same as previous pivot
            // put same elements to the left, and we do not have to recurse
            if (!leftmost && !comp(*(begin-1), *begin)) {
                begin = partition_to_left(begin, end, comp) + 1;
                continue;
            }

            auto part_result = branchless ? partition_right_branchless(begin, end, comp)
                                          : partition_to_right(begin, end, comp);
            RandomIt pivot_pos = part_result.first;
            const bool already_partitioned = part_result.second;

            diff_t l_size = pivot_pos - begin;
            diff_t r_size = end - (pivot_pos + 1);

            if (already_partitioned) {
                bool left = false;
                bool right = false;
                if (l_size > insertion_threshold)
                    left = leftmost ? partial_insertion_sort(begin, pivot_pos, comp) : partial_insertion_sort_unguarded(begin, pivot_pos, comp);
                if (r_size > insertion_threshold)
                    right = partial_insertion_sort_unguarded(pivot_pos + 1, end, comp);
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

            if (l_size < (size / parameters::partition_ratio) || r_size < (size / parameters::partition_ratio)) {
                // If we had too many bad partitions, switch to heapsort to guarantee O(n log n)
                if (--bad_allowed == 0) {
                    std::make_heap(begin, end, comp);
                    std::sort_heap(begin, end, comp);
                    return;
                }

                deterministic_shuffle(begin, end, l_size, r_size, pivot_pos, insertion_threshold);
            }

            seq_loop<RandomIt, Compare, branchless>(begin, pivot_pos, comp, bad_allowed, leftmost);
            leftmost = false;
            begin = ++pivot_pos;
        }
    }
}
