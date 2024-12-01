#pragma once

#include <bit>
#include <execution>

#ifndef NDEBUG
    #include <bitset>
    #include <iostream>
    #include <syncstream>
    #define TO_S(val) std::to_string(val)
    #define PRINT_ITERATORS(lp, rp, msg)                               \
        std::cout << msg << ": ";                                      \
        for (auto i = lp; i != (rp - 1); ++i) std::cout << *i << ", "; \
        std::cout << *(rp - 1) << std::endl;
    #define PRINT_ATOMIC_OMP(id, str) \
        _Pragma("omp critical") { std::cout << "my_id: " + std::to_string(id) + " " + str << std::endl << std::flush; }
    #define DO_SINGLE_OMP(func) \
        _Pragma("omp barrier") _Pragma("omp single") { func; }
    #define PRINT_VAL(str, val) (std::cout << str << ": " << std::to_string(val) << std::endl)
    #define PRINT_BIN(str, val) (std::cout << str << ": " << std::bitset<64>(val) << std::endl)

    #define PRINT_ATOMIC_CPP(str) \
        std::osyncstream(std::cout) << str << std::endl;
#else
    #define TO_S(val)
    #define PRINT_ITERATORS(lp, rp, msg)
    #define PRINT_ATOMIC_OMP(id, str)
    #define PRINT_VAL(str, val)
    #define PRINT_BIN(str, val)
    #define DO_SINGLE_OMP(func)
    #define PRINT_ATOMIC_CPP(str)
#endif

#ifdef PPQ_TIME_MEASURE
    #include <chrono>
    #define MEASURE_START(name) \
        const std::chrono::steady_clock::time_point name = std::chrono::steady_clock::now();
    #define MEASURE_END(name, msg)                                                                             \
        const std::chrono::steady_clock::time_point end_##name = std::chrono::steady_clock::now();                  \
        std::cout << msg << " " << std::chrono::duration_cast<std::chrono::milliseconds>(end_##name - name).count() \
                  << " ms" << std::endl
#else
    #define MEASURE_START(name)
    #define MEASURE_END(name_start, name_end, msg)
#endif

namespace ppqsort::parameters {
    #ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    #else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    #endif

    /**
     * @var insertion_threshold_primitive
     * @brief Threshold for switching to insertiton sort, when the type is primitive.
     */
    constexpr int insertion_threshold_primitive = 32;
    /**
     * @var insertion_threshold
     * @brief Threshold for switching to insertiton sort.
     */
    constexpr int insertion_threshold = 12;
    /**
     * @var partial_insertion_threshold
     * @brief Maximum swap count for partial insertion sort.
     */
    constexpr int partial_insertion_threshold = 8;
    /**
     * @var median_threshold
     * @brief Threshold for switching to median of three in pivot selection.
     */
    constexpr int median_threshold = 128;
    /**
     * @var partition_ratio
     * @brief Ratio used in computation for identifying bad partition.
     */
    constexpr int partition_ratio = 8;
    /**
     * @var cacheline_size
     * @brief Size of cacheline in bytes.
     */
    constexpr int cacheline_size = hardware_constructive_interference_size;
    /**
     * @var buffer_size
     * @brief Size of block for branchless partitioning.
     */
    constexpr int buffer_size = 1 << 10;
    using buffer_type = uint16_t;
    static_assert(std::numeric_limits<buffer_type>::max() > buffer_size,
                  "buffer_size is bigger than type for buffer can hold. This will overflow");
    /**
     * @var par_thr_div
     * @brief Division of threshold for computing sequential threshold (aka constant "k").
     */
    constexpr int par_thr_div = 8;
    /**
     * @var par_partition_block_size
     * @brief Size of block for parallel partitioning.
     */
    constexpr int par_partition_block_size = 1 << 14;
    /**
     * @var seq_threshold
     * @brief Minimal size for using parallel sort.
     */
    constexpr int seq_threshold = 1 << 18;
} // namespace ppqsort::parameters

namespace ppqsort::execution {
    class sequenced_policy {};
    class parallel_policy {};
    class sequenced_policy_force_branchless {};
    class parallel_policy_force_branchless {};

    /**
     * @var seq
     * @brief Sequential execution policy.
     */
    inline constexpr sequenced_policy seq{};
    /**
     * @var par
     * @brief Parallel execution policy.
     */
    inline constexpr parallel_policy par{};
    /**
     * @var seq_force_branchless
     * @brief Sequential execution policy with forced branchless partitioning.
     */
    inline constexpr sequenced_policy_force_branchless seq_force_branchless{};
    /**
     * @var par_force_branchless
     * @brief Parallel execution policy with forced branchless partitioning.
     */
    inline constexpr parallel_policy_force_branchless par_force_branchless{};

    template<typename T, typename U>
    inline constexpr bool _is_same_decay_v = std::is_same_v<std::decay_t<T>, std::decay_t<U>>;
} // namespace ppqsort::execution

namespace ppqsort::impl {
    enum side { left, right };

    inline int log2(const unsigned long long value) {
        // This is floor(log2(x))+1
        return std::bit_width(value);
    }
} // namespace ppqsort::impl
