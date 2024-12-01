/*
* TBD: license
*/

#pragma once

#include "ppqsort/mainloop.h"
#include "ppqsort/parameters.h"


#ifdef FORCE_CPP
    #include "ppqsort/parallel/cpp/mainloop_par.h"
#else
    #ifdef _OPENMP
        #include "ppqsort/parallel/openmp/mainloop_par.h"
    #else
        #include "ppqsort/parallel/cpp/mainloop_par.h"
    #endif
#endif


namespace ppqsort::impl {
   template <bool Force_branchless = false,
            typename RandomIt,
            typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>,
            bool Branchless = use_branchless<typename std::iterator_traits<RandomIt>::value_type, Compare>::value>
   inline void seq_ppqsort(RandomIt begin, RandomIt end, Compare comp = Compare()) {
       if (begin == end)
           return;

       constexpr bool branchless = Force_branchless || Branchless;
       seq_loop<RandomIt, Compare, branchless>(begin, end, comp, log2(end - begin));
   }

   template <typename ExecutionPolicy, typename... T>
   constexpr void call_sort(ExecutionPolicy&&, T... args) {
       static_assert(execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq)> ||
                     execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par)> ||
                     execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq_force_branchless)> ||
                     execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par_force_branchless)>,
           "Provided ExecutionPolicy is not valid. Use predefined policies from namespace ppqsort::execution.");
       if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq)>) {
           seq_ppqsort(std::forward<T>(args)...);
       } else if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par)>) {
           par_ppqsort(std::forward<T>(args)...);
       } else if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq_force_branchless)>) {
           seq_ppqsort<true>(std::forward<T>(args)...);
       } else if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par_force_branchless)>) {
           par_ppqsort<true>(std::forward<T>(args)...);
       } else {
           throw std::invalid_argument("Unknown execution policy."); // this should never happen
       }
   }
}

namespace ppqsort {
   template <typename RandomIt>
   void sort(RandomIt begin, RandomIt end) {
       impl::seq_ppqsort(begin, end);
   }

   /**
    * @Brief Sorts the elements in the range [begin, end). Will use std::less as comparator and std::thread::hardware_concurrency() as the number of threads.
    * @tparam ExecutionPolicy
    * @tparam RandomIt
    * @param policy Defined execution policy. Use predefined policies from namespace ppqsort::execution.
    * @param begin Iterator to the first element in the range.
    * @param end Iterator to the element past the last element in the range.
    */
   template <typename ExecutionPolicy, typename RandomIt>
   void sort(ExecutionPolicy&& policy, RandomIt begin, RandomIt end) {
       impl::call_sort(std::forward<ExecutionPolicy>(policy), begin, end);
   }

   /**
    * @brief Sorts the elements in the range [begin, end) utilizing the specified number of threads.
    * @tparam ExecutionPolicy
    * @tparam RandomIt
    * @param policy Defined execution policy. Use predefined policies from namespace ppqsort::execution.
    * @param begin Iterator to the first element in the range.
    * @param end Iterator to the element past the last element in the range.
    * @param threads Number of threads to use for sorting.
    */
   template <typename ExecutionPolicy, typename RandomIt>
   void sort(ExecutionPolicy&& policy, RandomIt begin, RandomIt end, const int threads) {
       impl::call_sort(std::forward<ExecutionPolicy>(policy), begin, end, threads);
   }

   /**
    * @brief Sorts the elements in the range [begin, end) using provided comparator. Will run sequentially.
    * @tparam RandomIt
    * @tparam Compare
    * @param begin Iterator to the first element in the range.
    * @param end Iterator to the element past the last element in the range.
    * @param comp Comparator to use for sorting.
    */
   template <typename RandomIt, typename Compare>
   void sort(RandomIt begin, RandomIt end, Compare comp) {
       impl::seq_ppqsort(begin, end, comp);
   }

   /**
    * @brief Sorts the elements in the range [begin, end) using provided comparator.
    * @tparam ExecutionPolicy
    * @tparam RandomIt
    * @tparam Compare
    * @param policy Defined execution policy. Use predefined policies from namespace ppqsort::execution.
    * @param begin Iterator to the first element in the range.
    * @param end Iterator to the element past the last element in the range.
    * @param comp Comparator to use for sorting.
    */
   template <typename ExecutionPolicy, typename RandomIt, typename Compare>
   void sort(ExecutionPolicy&& policy, RandomIt begin, RandomIt end, Compare comp) {
       impl::call_sort(std::forward<ExecutionPolicy>(policy), begin, end, comp);
   }

   /**
    * @brief Sorts the elements in the range [begin, end) using provided comparator and utilizing the specified number of threads.
    * @tparam ExecutionPolicy
    * @tparam RandomIt
    * @tparam Compare
    * @param policy Defined execution policy. Use predefined policies from namespace ppqsort::execution.
    * @param begin Iterator to the first element in the range.
    * @param end Iterator to the element past the last element in the range.
    * @param comp Comparator to use for sorting.
    * @param threads Number of threads to use for sorting.
    */
   template <typename ExecutionPolicy, typename RandomIt, typename Compare>
   void sort(ExecutionPolicy&& policy, RandomIt begin, RandomIt end, Compare comp, const int threads) {
       impl::call_sort(std::forward<ExecutionPolicy>(policy), begin, end, comp, threads);
   }
}