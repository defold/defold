
#ifndef DM_ATOMIC_H
#define DM_ATOMIC_H

/*
 * 64-bit notes
 *  - On win32 use Interlocked*64
 */

// platform independent includes
#include <stdint.h>

// platform specific includes
#if defined(__PS3_GCC_REVISION__)
#include <cell/atomic.h>
#include <ppu_intrinsics.h>
#elif defined(XENON)
#include <xtl.h>
#elif defined(_MSC_VER)
#include "safe_windows.h"
#endif

//! Type definitions of atomic types
typedef volatile int32_t int32_atomic_t;

/**
 * Atomic increment of a int32_atomic_t.
 * @param ptr Pointer to a int32_atomic_t to increment.
 * @return Previous value
 */
inline int32_t dmAtomicIncrement32(int32_atomic_t* ptr)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	int32_t tmp[32] __attribute__((aligned(128)));
	int32_t result = cellAtomicIncr32(tmp, (unsigned long *) ptr);
#else
	int32_t result = cellAtomicIncr32((unsigned long *) ptr);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return InterlockedIncrement((volatile long*) ptr)-1;
#else
	return __sync_fetch_and_add((int32_atomic_t*) ptr, 1);
#endif
}

/**
 * Atomic decrement of a int32_atomic_t.
 * @param ptr Pointer to a int32_atomic_t to decrement.
 * @return Previous value
 */
inline int32_t dmAtomicDecrement32(int32_atomic_t* ptr)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	int32_t tmp[32] __attribute__((aligned(128)));
	int32_t result = cellAtomicDecr32(tmp, (unsigned long *) ptr);
#else
	int32_t result = cellAtomicDecr32((unsigned long *) ptr);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return InterlockedDecrement((volatile long*) ptr)+1;
#else
	return __sync_fetch_and_sub((int32_atomic_t*) ptr, 1);
#endif
}

/**
 * Atomic addition of a int32_atomic_t.
 * @param ptr Pointer to a int32_atomic_t to add.
 * @param value Value to add.
 * @return Previous value
 */
inline int32_t dmAtomicAdd32(int32_atomic_t *ptr, int32_t value)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	int32_t tmp[32] __attribute__((aligned(128)));
	int32_t result = cellAtomicAdd32(tmp, (unsigned long *) ptr, (unsigned long) value);
#else
	int32_t result = cellAtomicAdd32((unsigned long *) ptr, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return InterlockedExchangeAdd((volatile long*) ptr, (long) value);
#else
	return __sync_fetch_and_add((int32_atomic_t*) ptr, (long) value);
#endif
}

/**
 * Atomic subtraction of a int32_atomic_t.
 * @param ptr Pointer to a int32_atomic_t to subtract.
 * @param value Value to subtract.
 * @return Previous value
 */
inline int32_t dmAtomicSub32(int32_atomic_t *ptr, int32_t value)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	int32_t tmp[32] __attribute__((aligned(128)));
	int32_t result = cellAtomicSub32(tmp, (unsigned long *) ptr, (unsigned long) value);
#else
	int32_t result = cellAtomicSub32((unsigned long *) ptr, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return InterlockedExchangeAdd((volatile long*) ptr, -((long)value));
#else
	return __sync_fetch_and_sub((int32_atomic_t*) ptr, (long) value);
#endif
}

/**
 * Atomic exchange of a int32_atomic_t.
 * @param ptr Pointer to a int32_atomic_t to store into.
 * @param value Value to store.
 * @return Previous value.
 */
inline int32_t dmAtomicStore32(int32_atomic_t *ptr, int32_t value)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	int32_t tmp[32] __attribute__((aligned(128)));
	int32_t result = cellAtomicStore32(tmp, (unsigned long *) ptr, (unsigned long) value);
#else
	int32_t result = cellAtomicStore32((unsigned long *) ptr, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return InterlockedExchange((volatile long*) ptr, (long) value);
#else
	return __sync_lock_test_and_set((int32_atomic_t*) ptr, value);
#endif
}

/**
 * Atomic exchange of a int32_atomic_t if comparand is equal to the value of #ptr
 * @param ptr Pointer to a int32_atomic_t to store into.
 * @param value Value to store.
 * @param comparand Value to compare to.
 * @return Previous value
 */
inline int32_t dmAtomicCompareStore32(int32_atomic_t *ptr, int32_t value, int32_t comparand)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	int32_t tmp[32] __attribute__((aligned(128)));
	int32_t result = cellAtomicCompareAndSwap32(tmp, (unsigned long *) ptr, (unsigned long) comparand, (unsigned long) value);
#else
	int32_t result = cellAtomicCompareAndSwap32((unsigned long *) ptr, (unsigned long) comparand, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return InterlockedCompareExchange((volatile long*) ptr, (long) value, (long) comparand);
#else
	return __sync_val_compare_and_swap((int32_atomic_t*) ptr, comparand, value);
#endif
}

#endif //DM_ATOMIC_H
