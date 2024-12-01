#pragma once

namespace ppqsort::impl {
    /**
     * @Brief        Sort 2 elements, 1 compare, branchless
     * @Note         https://godbolt.org/z/o3sTjeGnv
     * @tparam RandomIt
     * @tparam Compare
     * @tparam T
     * @param a
     * @param b
     * @param comp
     */
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void sort_2_branchless(RandomIt a, RandomIt b, Compare comp) {
        bool res = comp(*a, *b);
        T tmp = res ? *a : *b;
        *b = res ? *b : *a;
        *a = tmp;
    }

    /**
     * @Brief        Sort 3 elements, 2 compares, branchless
     * @Note         Assumes that b and c are already sorted
     * @tparam RandomIt
     * @tparam Compare
     * @tparam T
     * @param a
     * @param b
     * @param c
     * @param comp
     */
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void sort_3_partial_branchless(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
        bool res = comp(*c, *a);
        T tmp = res ? *c : *a;
        *c = res ? *a : *c;
        res = comp(tmp, *b);
        *a = res ? *a : *b;
        *b = res ? *b : tmp;
    }

    // 17 instructions, no jumps, 3 compares

    /**
     * @Brief        Sort 3 elements, 3 compares, branchless, 17 instructions
     * @tparam RandomIt
     * @tparam Compare
     * @param a
     * @param b
     * @param c
     * @param comp
     */
    template<class RandomIt, class Compare>
    inline void sort_3_branchless(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
        sort_2_branchless(b, c, comp);
        sort_3_partial_branchless(a, b, c, comp);
    }

    // 43 instructions, no jumps, 9 compares
    /**
     * @Brief        Sort 5 elements, 9 compares, branchless, 43 instructions
     * @tparam RandomIt
     * @tparam Compare
     * @param x1
     * @param x2
     * @param x3
     * @param x4
     * @param x5
     * @param comp
     */
    template<class RandomIt, class Compare>
    inline void sort_5_branchless(RandomIt x1, RandomIt x2, RandomIt x3, RandomIt x4, RandomIt x5, Compare comp) {
        sort_2_branchless(x1, x2, comp);
        sort_2_branchless(x4, x5, comp);
        sort_3_partial_branchless(x3, x4, x5, comp);
        sort_2_branchless(x2, x5, comp);
        sort_3_partial_branchless(x1, x3, x4, comp);
        sort_3_partial_branchless(x2, x3, x4, comp);
    }

    // 2-3 compares, 0-2 swaps, stable
    /**
     * @Brief        Sort 2 elements, maximum 3 compares and 2 swaps, minimum 2 compares and 0 swap
     * @tparam RandomIt
     * @tparam Compare
     * @param a
     * @param b
     * @param c
     * @param comp
     */
    template<class RandomIt, class Compare>
    inline void sort_3(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
        if (!comp(*b, *a)) {     // a <= b
            if (!comp(*c, *b))   // b <= c
                return;
            // b >= a, but b > c --> swap
            std::iter_swap(b, c);
            // after swap: a <= c, b < c
            if (comp(*b, *a)) {
                // a <= c, b < c, a > b
                std::iter_swap(a, b);
                // after swap: b <= c, a < c, a < b
            }
            return;
        }
        if (comp(*c, *b)) {
            // a > b, b > c
            std::iter_swap(a, c);
            return;
        }
        // a > b, b <= c
        std::iter_swap(a, b);
        // a < b, a <= c
        if (comp(*c, *b)) {
            // a < b, a <= c, b > c
            std::iter_swap(b, c);
            // after swap: a < c, a <= b, b < c
        }
    }

        /**
     * @Brief        Sort range using insertion sort
     * @tparam RandomIt     Random access iterator
     * @tparam Compare      Comparator
     * @tparam T            Value type of RandomIt
     * @param begin         Start of range
     * @param end           End of range
     * @param comp          Comparator
     */
    template<typename RandomIt, typename Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void insertion_sort(RandomIt begin, RandomIt end, Compare comp) {
        if (begin == end)
            return;

        // before iterator "first" is sorted segment
        // under and after iterator "second" is unsorted segment
        // | ___ sorted ___ <first> <second> ___ unsorted ___ |
        for (auto it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                // save current element
                T elem = std::move(*second);
                // iterate until according spot is found
                do {
                    *second-- = std::move(*first--);
                } while (second != begin && comp(elem, *first));
                // copy current element
                *second = std::move(elem);
            }
        }
    }

    /**
     * @Brief        Sort range using insertion sort
     * @Note         Assumes that element before begin is lower or equal to all elements in range
     * @tparam RandomIt     Random access iterator
     * @tparam Compare      Comparator
     * @tparam T            Value type of RandomIt
     * @param begin         Start of range
     * @param end           End of range
     * @param comp          Comparator
     */
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void insertion_sort_unguarded(RandomIt begin, RandomIt end, Compare comp) {
        if (begin == end)
            return;

        // same approach as for insertion sort above
        // only one check in while loop can be omitted
        for (RandomIt it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                T elem = std::move(*second);
                do {
                    *second-- = std::move(*first--);
                } while (comp(elem, *first));
                *second = std::move(elem);
            }
        }
    }

    /**
     * @Brief        Try to sort range using insertion sort with limit on swaps
     * @Note         Assumes begin < end
     * @tparam RandomIt     Random access iterator
     * @tparam Compare    Comparator
     * @tparam T        Value type of RandomIt
     * @param begin     Start of range
     * @param end       End of range
     * @param comp      Comparator
     * @return        True if sorting was successful, false if aborted
     */
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline bool partial_insertion_sort(RandomIt begin, RandomIt end, Compare comp) {
        constexpr unsigned int swap_limit = parameters::partial_insertion_threshold;
        std::size_t count = 0;

        for (RandomIt it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                T elem = std::move(*second);

                do {
                    *second-- = std::move(*first--);
                } while (second != begin && comp(elem, *first));
                *second = std::move(elem);
                if (++count >= swap_limit) {
                    return ++it == end;
                }
            }
        }
        return true;
    }

    /**
     * @Brief        Try to sort range using insertion sort with limit on swaps
     * @Note         Assumes begin < end and that element before begin is lower or equal to all elements in range
     * @tparam RandomIt     Random access iterator
     * @tparam Compare    Comparator
     * @tparam T        Value type of RandomIt
     * @param begin     Start of range
     * @param end       End of range
     * @param comp      Comparator
     * @return        True if sorting was successful, false if aborted
     */
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline bool partial_insertion_sort_unguarded(RandomIt begin, RandomIt end, Compare comp) {
        constexpr unsigned int swap_limit = parameters::partial_insertion_threshold;
        std::size_t count = 0;

        for (RandomIt it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                T elem = std::move(*second);

                do {
                    *second-- = std::move(*first--);
                } while (comp(elem, *first));
                *second = std::move(elem);
                if (++count >= swap_limit) {
                    return ++it == end;
                }
            }
        }
        return true;
    }
}