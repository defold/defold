#pragma once

namespace ppqsort::impl::openmp {

    #if (defined(__GNUC__) && !defined(__clang__)) && !defined(__INTEL_COMPILER)
        #define GCC_COMPILER
    #endif

    template <side s, typename diff_t>
    inline bool get_new_block(diff_t& t_size, diff_t& t_iter, diff_t& t_block_bound,
                              diff_t& g_distance, diff_t& g_offset, const int& block_size) {
        #pragma omp atomic capture
        { t_size = g_distance; g_distance -= block_size; }
        if (t_size < block_size) {
            // last block not aligned
            #pragma omp atomic update
            g_distance += block_size;
            return false;
        } else {
            if constexpr (s == side::left) {
                // left block is clean, get another one
                #pragma omp atomic capture
                { t_iter = g_offset; g_offset += block_size; }
                t_block_bound = t_iter + (block_size - 1);
                return true;
            } else {
                #pragma omp atomic capture
                { t_iter = g_offset; g_offset -= block_size; }
                t_block_bound = t_iter - (block_size - 1);
                return true;
            }
        }
    }

    template <side s, typename RandomIt, typename diff_t>
    inline void swap_blocks(const int& g_dirty_blocks_side, const RandomIt& g_begin,
                            const diff_t& t_old, const diff_t& t_block_bound,
                            std::unique_ptr<bool[]>& reserved, const int& block_size)
    {
        diff_t swap_start = -1;
        for (int i = 0; i < g_dirty_blocks_side; ++i) {
            // find clean block in dirty segment
            // GCC >= 12 and clang >= 17 supports "atomic compare" from OpenMP 5.1
            bool t_reserve_success = false;
            #if (defined(GCC_COMPILER) && (__GNUC__ >= 12)) || (defined(__clang__) && (__clang_major__ >= 17))
            #pragma omp atomic compare capture
            {
                t_reserve_success = reserved[i];
                if (reserved[i] == false) {
                    reserved[i] = true;
                }
            }
            t_reserve_success = !t_reserve_success;
            #else
            if (reserved[i] == false) {
                #pragma omp critical
                if (reserved[i] == false) {
                    t_reserve_success = reserved[i] = true;
                }
            }
            #endif
            if (t_reserve_success) {
                if constexpr (s == side::left)
                    swap_start = t_old - (i + 1) * block_size;
                else
                    swap_start = t_old + i * block_size + 1;
                break;
            }
        }
        RandomIt block_start, block_end;
        if constexpr (s == side::left) {
            block_start = g_begin + t_block_bound - (block_size - 1);
            block_end = g_begin + t_block_bound + 1;
        } else {
            block_start = g_begin + t_block_bound;
            block_end = g_begin + t_block_bound + block_size;
        }
        // swap our dirty block with found clean block
        std::swap_ranges(block_start, block_end, g_begin + swap_start);
    }

    template <typename RandomIt, typename diff_t>
    inline void swap_dirty_blocks(const RandomIt& g_begin, const diff_t & t_left, const diff_t & t_right,
                                  const diff_t & t_left_end, const diff_t & t_right_start,
                                  int & g_dirty_blocks_left, int & g_dirty_blocks_right,
                                  diff_t & g_first_offset, diff_t & g_last_offset,
                                  std::unique_ptr<bool[]>& g_reserved_left,
                                  std::unique_ptr<bool[]>& g_reserved_right,
                                  const int& block_size)
    {
        const bool t_dirty_left = t_left <= t_left_end;
        const bool t_dirty_right = t_right >= t_right_start;

        // count and clean dirty blocks
        // maximum of dirty blocks is thread count
        if (t_dirty_left) {
            #pragma omp atomic update
            ++g_dirty_blocks_left;
        }
        if (t_dirty_right) {
            #pragma omp atomic update
            ++g_dirty_blocks_right;
        }

        #pragma omp barrier

        // create new pointers which will separate clean and dirty blocks
        const diff_t t_first_old = g_first_offset;
        const diff_t t_first_clean = g_first_offset - g_dirty_blocks_left * block_size;
        const diff_t t_last_old = g_last_offset;
        const diff_t t_last_clean = g_last_offset + g_dirty_blocks_right * block_size;

        // reserve spots for dirty blocks
        // g_reserved_<side> is for dirty blocks that will be in the middle
        if (t_dirty_left && t_left_end >= t_first_clean) {
            // Block is dirty and in dirty segment --> reserve spot
            g_reserved_left[(t_first_old - (t_left_end + 1)) / block_size] = 1;
        }

        if (t_dirty_right && t_right_start <= t_last_clean) {
            g_reserved_right[((t_right_start - 1) - t_last_old) / block_size] = 1;
        }

        // otherwise block is clean and in dirty segment
        // do not reserve spot --> g_reserved_<side>[b] = 0

        #pragma omp barrier
        if (t_dirty_left && t_left_end < t_first_clean) {
            // dirty block in clean segment --> swap
            swap_blocks<side::left>(g_dirty_blocks_left, g_begin,
                                    t_first_old, t_left_end, g_reserved_left, block_size);
        }
        if (t_dirty_right && t_right_start > t_last_clean) {
            swap_blocks<side::right>(g_dirty_blocks_right, g_begin, t_last_old,
                                     t_right_start, g_reserved_right, block_size);
        }

        // update global offsets so it separates clean and dirty segment
        // all threads after last barrier will not write to these offsets
        // it is safe to do it non atomically
        #pragma omp single
        {
            g_first_offset = t_first_clean;
            g_last_offset = t_last_clean;
        }
    }

    template <typename RandomIt, typename Compare,
            typename T = typename std::iterator_traits<RandomIt>::value_type,
            typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline std::pair<RandomIt, bool> partition_to_right_par(const RandomIt& g_begin,
                                                             const RandomIt& g_end,
                                                             const Compare& comp,
                                                             const int& thread_count) {
        constexpr int block_size = parameters::par_partition_block_size;
        const diff_t g_size = g_end - g_begin;
        diff_t g_distance = g_size - 1;
        
        // at least 2 blocks for each thread
        if (g_distance < 2 * block_size * thread_count)
            return partition_to_right(g_begin, g_end, comp);

        T pivot = std::move(*g_begin);
        diff_t g_first_offset = 1;
        diff_t g_last_offset = g_size - 1;

        // counters for dirty blocks
        int g_dirty_blocks_left = 0;
        int g_dirty_blocks_right = 0;

        // helper arrays for cleaning blocks
        std::unique_ptr<bool[]> g_reserved_left(new bool[thread_count]{false});
        std::unique_ptr<bool[]> g_reserved_right(new bool[thread_count]{false});
        bool g_already_partitioned = true;

        // reserve first blocks for each thread
        g_first_offset += block_size * thread_count;
        g_last_offset -= block_size * thread_count;
        g_distance -= block_size * thread_count * 2;

        // prefix g_ is for shared variables
        // prefix t_ is for private variables

        // each thread will get two blocks (from start and from end)
        // and neutralize it
        #pragma omp parallel num_threads(thread_count)
        {
            // each thread will get first two blocks without collisions
            const int t_my_id = omp_get_thread_num();

            // iterators for given block
            diff_t t_left = (block_size * t_my_id) + 1;
            diff_t t_right = (g_size-1) - block_size * t_my_id;

            // block borders
            diff_t t_left_end = t_left + block_size - 1;
            diff_t t_right_start = t_right - block_size + 1;

            diff_t t_size = 0;
            bool t_already_partitioned = true;

            while (true) {
                // get new blocks if needed or end partitioning
                if (t_left > t_left_end) {
                    if (!get_new_block<side::left>(t_size, t_left, t_left_end,
                                                   g_distance, g_first_offset, block_size))
                        break;
                }
                if (t_right < t_right_start) {
                    if (!get_new_block<side::right>(t_size, t_right, t_right_start,
                                                    g_distance, g_last_offset, block_size))
                        break;
                }

                // find first element from block start, which is greater or equal to pivot
                // we must guard it, because in block nothing is guaranteed
                while (t_left <= t_left_end && comp(g_begin[t_left], pivot)) {
                    ++t_left;
                }
                // find first elem from block g_end, which is less than pivot
                while (t_right >= t_right_start && !comp(g_begin[t_right], pivot)) {
                    --t_right;
                }

                // do swaps
                while (t_left < t_right) {
                    if (t_left > t_left_end || t_right < t_right_start)
                        break;
                    std::iter_swap(g_begin + t_left, g_begin + t_right);
                    t_already_partitioned = false;
                    while (++t_left <= t_left_end && comp(g_begin[t_left], pivot));
                    while (--t_right >= t_right_start && !comp(g_begin[t_right], pivot));
                }
            } // End of parallel partition main loop
            #pragma omp atomic update
            g_already_partitioned &= t_already_partitioned;

            // swaps dirty blocks from clean segment to dirty segment
            swap_dirty_blocks(g_begin, t_left, t_right, t_left_end, t_right_start,
                              g_dirty_blocks_left, g_dirty_blocks_right,
                              g_first_offset, g_last_offset, g_reserved_left, g_reserved_right,
                              block_size);
        }  // End of a parallel section

        // do sequential cleanup of dirty segment
        return seq_cleanup<false>(g_begin, pivot, comp, g_first_offset, g_last_offset, g_already_partitioned);
    }
}
