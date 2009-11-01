
#ifndef DM_ATOMIC_H
#define DM_ATOMIC_H

// platform independent includes
#include <stdint.h>

// platform specific includes
#if defined(__PS3_GCC_REVISION__)
#include <cell/atomic.h>
#include <ppu_intrinsics.h>
#elif defined(XENON)
#include <xtl.h>
#elif defined(_MSC_VER)
#include <windows.h>
#endif

//! Type definitions of atomic types
typedef volatile uint32_t uint32_atomic_t;

/**
 * @fn dmAtomicIncrement32
 * Atomic increment of a uint32_atomic_t.
 * @ptr Pointer to a uint32_atomic_t to increment.
 * @return Result of increment.
 */
inline uint32_t dmAtomicIncrement32(uint32_atomic_t* ptr)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	uint32_t tmp[32] __attribute__((aligned(128)));
	uint32_t result = (uint32_t) cellAtomicIncr32(tmp, (unsigned long *) ptr)+1;
#else
	uint32_t result = (uint32_t) cellAtomicIncr32((unsigned long *) ptr)+1;
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return (uint32_t) InterlockedIncrementAcquire((volatile long*) ptr);
#else
	return (uint32_t) __sync_add_and_fetch((volatile long*) ptr, 1);
#endif
}


/**
 * @fn dmAtomicDecrement32
 * Atomic decrement of a uint32_atomic_t.
 * @ptr Pointer to a uint32_atomic_t to decrement.
 * @return Result of decrement.
 */
inline uint32_t dmAtomicDecrement32(uint32_atomic_t* ptr)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	uint32_t tmp[32] __attribute__((aligned(128)));
	uint32_t result = (uint32_t) cellAtomicDecr32(tmp, (unsigned long *) ptr)-1;
#else
	uint32_t result = (uint32_t) cellAtomicDecr32((unsigned long *) ptr)-1;
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return (uint32_t) InterlockedDecrementAcquire((volatile long*) ptr);
#else
	return (uint32_t) __sync_sub_and_fetch((volatile long*) ptr, 1);
#endif
}


/**
 * @fn dmAtomicAdd32
 * Atomic addition of a uint32_atomic_t.
 * @ptr Pointer to a uint32_atomic_t to add.
 * @value Value to add.
 * @return Previous value.
 */
inline uint32_t dmAtomicAdd32(uint32_atomic_t *ptr, uint32_t value)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	uint32_t tmp[32] __attribute__((aligned(128)));
	uint32 result = (uint32)cellAtomicAdd32(tmp, (unsigned long *) ptr, (unsigned long) value);
#else
	uint32 result = (uint32)cellAtomicAdd32((unsigned long *) ptr, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return (uint32_t) InterlockedExchangeAdd((volatile long*) ptr, (long) value);
#else
	return (uint32_t) __sync_fetch_and_add((volatile long*) ptr, (long) value);
#endif
}


/**
 * @fn dmAtomicSub32
 * Atomic subtraction of a uint32_atomic_t.
 * @ptr Pointer to a uint32_atomic_t to subtract.
 * @value Value to subtract.
 * @return Previous value.
 */
inline uint32_t dmAtomicSub32(uint32_atomic_t *ptr, uint32_t value)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	uint32_t tmp[32] __attribute__((aligned(128)));
	uint32_t result = (uint32_t) cellAtomicSub32(tmp, (unsigned long *) ptr, (unsigned long) value);
#else
	uint32_t result = (uint32_t) cellAtomicSub32((unsigned long *) ptr, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return (uint32_t) InterlockedExchangeAdd((volatile long*) ptr, -((long)value));
#else
	return (uint32_t) __sync_fetch_and_sub((volatile long*) ptr, (long) value);
#endif
}


/**
 * @fn dmAtomicStore32
 * Atomic exchange of a uint32_atomic_t.
 * @ptr Pointer to a uint32_atomic_t to store into.
 * @value Value to store.
 * @return Previous value.
 */
inline uint32_t dmAtomicStore32(uint32_atomic_t *ptr, uint32_t value)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	uint32_t tmp[32] __attribute__((aligned(128)));
	uint32 result = (uint32_t) cellAtomicStore32(tmp, (unsigned long *) ptr, (unsigned long) value);
#else
	uint32 result = (uint32_t) cellAtomicStore32((unsigned long *) ptr, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return (uint32_t) InterlockedExchange((volatile long*) ptr, (long) value);
#else
	return (uint32_t) __sync_lock_test_and_set((volatile long*) ptr, value);
#endif
}


/**
 * @fn dmAtomicCompareStore32
 * Atomic exchange of a uint32_atomic_t if comperand is equal to (destination) address.
 * @ptr Pointer to a uint32_atomic_t to store into.
 * @value Value to store.
 * @comperand Value to compare to.
 * @return Previous value.
 */
inline uint32_t dmAtomicCompareStore32(uint32_atomic_t *ptr, uint32_t value, uint32_t comperand)
{
#if defined(__PS3_GCC_REVISION__)
#if defined(__SPU__)
	uint32_t tmp[32] __attribute__((aligned(128)));
	uint32 result = (uint32_t) cellAtomicCompareAndSwap32(tmp, (unsigned long *) ptr, (unsigned long) comperand, (unsigned long) value);
#else
	uint32 result = (uint32_t) cellAtomicCompareAndSwap32((unsigned long *) ptr, (unsigned long) comperand, (unsigned long) value);
#endif
	__lwsync();
	return result;
#elif defined(_MSC_VER)
	return (uint32_t) InterlockedCompareExchange((volatile long*) ptr, (long) value, (long) comperand);
#else
	return (uint32_t) __sync_lock_test_and_set((volatile long*) ptr, comperand, value);
#endif
}

#endif //DM_ATOMIC_H
