// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.util;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.*;

// Sources:
// https://www.scaler.com/topics/data-structures/open-addressing
// https://planetmath.org/goodhashtableprimes

public final class WeakInterner<T> {
    private static final float DEFAULT_LOAD_FACTOR = 0.75f;
    private static final int[] PRIME_CAPACITY_SEQUENCE;
    private static final int MAX_CAPACITY;

    private Entry<T>[] hashTable;
    private int count;
    private int growthThreshold;
    private final float loadFactor;
    private final ReferenceQueue<T> staleEntriesQueue;
    private final Entry<T> removedSentinelEntry;

    static {
        final ArrayList<Integer> primeCapacitySequence = new ArrayList<>();

        for (int i = 0, len = 30; i < len; ++i) {
            final int powerOfTwo = 1 << (i + 1);
            final boolean addSubsteps = i > 21;

            if (addSubsteps) {
                primeCapacitySequence.add(getNextPrime(powerOfTwo + powerOfTwo / 4));
            }

            primeCapacitySequence.add(getNextPrime(powerOfTwo + powerOfTwo / 2));

            if (addSubsteps) {
                primeCapacitySequence.add(getNextPrime(powerOfTwo + powerOfTwo / 4 * 3));
            }
        }

        PRIME_CAPACITY_SEQUENCE = new int[primeCapacitySequence.size()];

        for (int i = 0, len = PRIME_CAPACITY_SEQUENCE.length; i < len; ++i) {
            PRIME_CAPACITY_SEQUENCE[i] = primeCapacitySequence.get(i);
        }

        MAX_CAPACITY = PRIME_CAPACITY_SEQUENCE[PRIME_CAPACITY_SEQUENCE.length - 1];
    }

    /**
     * Constructs a new WeakInterner with the specified initial capacity and a load factor of 0.75.
     * @param initialCapacity The initial number of values we can intern before growing the internal storage.
     */
    public WeakInterner(final int initialCapacity) {
        this(initialCapacity, DEFAULT_LOAD_FACTOR);
    }

    /**
     * Constructs a new WeakInterner with the specified initial capacity and load factor.
     * @param initialCapacity The initial number of values we can intern before growing the internal storage.
     * @param loadFactor The ratio of accepted occupancy before growing the internal storage.
     */
    public WeakInterner(final int initialCapacity, final float loadFactor) {
        if (loadFactor <= 0.0f || Float.isNaN(loadFactor)) {
            throw new IllegalArgumentException("Load factor must be a positive number.");
        }

        final int capacity = initialCapacity == 0 ? 0 : toPrimeCapacity(initialCapacity);
        this.hashTable = makeHashTable(capacity);
        this.count = 0;
        this.loadFactor = loadFactor;
        this.growthThreshold = (int) (capacity * loadFactor);
        this.staleEntriesQueue = new ReferenceQueue<>();
        this.removedSentinelEntry = new Entry<>(null, null, 0);
    }

    /**
     * Returns an immutable list containing all the live values currently in the
     * WeakInterner. This can be used during development to inspect its
     * contents. It is not recommended to invoke this method for purposes other
     * than debugging.
     * @return An unmodifiable list containing all the live values.
     */
    public synchronized List<Object> getValues() {
        // Obtain a cleaned hash table, ensuring anything in the stale entries
        // queue has been processed before we start to iterate through entries.
        final Entry<T>[] hashTable = getHashTable();
        final ArrayList<Object> liveValues = new ArrayList<>(count);

        for (Entry<T> entry : hashTable) {
            if (entry != null && entry != removedSentinelEntry) {
                final Object value = entry.get();

                if (value != null) {
                    liveValues.add(value);
                }
            }
        }

        liveValues.trimToSize();
        return Collections.unmodifiableList(liveValues);
    }

    /**
     * Returns a nested map of details about the internals of the WeakInterner.
     * This can be used during development to inspect resource allocation, etc.
     * @return A nested unmodifiable map with details about the WeakInterner.
     */
    public synchronized Map<String, Object> getDebugInfo() {
        final int capacity = hashTable.length;
        final List<Map<String, Object>> hashTableInfos = new ArrayList<>(capacity);

        for (Entry<T> entry : hashTable) {
            Map<String, Object> entryInfo;

            if (entry == null) {
                entryInfo = null;
            } else if (entry == removedSentinelEntry) {
                entryInfo = Map.of("status", "removed", "hash-value", entry.hashValue, "attempt", 0, "elapsed", 0);
            } else {
                final int hashValue = entry.hashValue;
                final int idealSlotIndex = getAbsoluteModulus(hashValue, capacity);
                final int slotConflictOffset = getSlotConflictOffset(hashValue, capacity);
                int attempt = 0;
                final long startTime = System.nanoTime();

                while (attempt < capacity) {
                    final int index = getAbsoluteModulus(idealSlotIndex + slotConflictOffset * attempt, capacity);

                    if (hashTable[index] == entry) {
                        break;
                    }

                    ++attempt;
                }

                final long elapsed = System.nanoTime() - startTime;
                final Object value = entry.get();
                entryInfo = value == null
                        ? Map.of("status", "stale", "hash-value", hashValue, "attempt", attempt, "elapsed", elapsed)
                        : Map.of("status", "valid", "hash-value", hashValue, "attempt", attempt, "elapsed", elapsed, "value", value);
            }

            hashTableInfos.add(entryInfo);
        }

        return Map.of(
                "count", count,
                "growth-threshold", growthThreshold,
                "growth-sequence", Arrays.stream(PRIME_CAPACITY_SEQUENCE).boxed().toList(),
                "hash-table", Collections.unmodifiableList(hashTableInfos)
        );
    }

    /**
     * Returns a canonical representation of an immutable value.
     * Multiple calls to this method with equivalent values will return the
     * previous instance. This can be used to ensure we only have a single
     * canonical representation of a value instead of many duplicate instances.
     * Values are retained as WeakReferences, and storage will be reclaimed once
     * the canonical values are no longer reachable outside the WeakInterner.
     * @param value The immutable value to intern.
     * @return The canonical representation of the supplied value.
     */
    public T intern(final T value) {
        if (value == null) {
            throw new IllegalArgumentException("The interned value cannot be null.");
        }

        // First attempt to get the existing value without locking. If we don't
        // find a match, lock access from other threads while adding it.
        final int hashValue = value.hashCode();
        final Entry<T>[] hashTable = getHashTable();
        final T existingValue = findExistingValue(hashTable, value, hashValue);
        return existingValue != null ? existingValue : internSynchronized(value, hashValue);
    }

    private synchronized T internSynchronized(final T value, final int hashValue) {
        // Another thread might have modified the hash table while we were
        // waiting for it to release the lock. Thus, we must get the hash table
        // again and try to find an existing value once more before interning it
        // ourselves.
        {
            final Entry<T>[] hashTable = getHashTable();
            final T existingValue = findExistingValue(hashTable, value, hashValue);

            if (existingValue != null) {
                // Another thread interned the value while we were waiting.
                return existingValue;
            }

            final int capacity = hashTable.length;

            if (count == MAX_CAPACITY) {
                // We're full, and already at MAX_CAPACITY, so unable to grow.
                // Simply return the input value without interning it.
                return value;
            }

            // We get to intern the value. Make sure we have room.
            if (count + 1 >= growthThreshold) {
                final int newCapacity = toPrimeCapacity(capacity + 1);
                rehash(newCapacity);
            }
        }

        // Note: The rehash() method might have updated the hashTable field, so
        // we need to get a fresh reference to it.
        final Entry<T>[] hashTable = getHashTable();
        final int capacity = hashTable.length;
        final int idealSlotIndex = getAbsoluteModulus(hashValue, capacity);
        final int slotConflictOffset = getSlotConflictOffset(hashValue, capacity);
        assert count < capacity;

        for (int attempt = 0; attempt < capacity; ++attempt) {
            final int index = getAbsoluteModulus(idealSlotIndex + slotConflictOffset * attempt, capacity);
            final Entry<T> existingEntry = hashTable[index];

            if (existingEntry == null || existingEntry == removedSentinelEntry || existingEntry.refersTo(null)) {
                hashTable[index] = new Entry<>(value, staleEntriesQueue, hashValue);
                ++count;
                break;
            }
        }

        // Note: We might reach this point without having successfully interned
        // the value if the hash table is full. If so, we simply return the
        // value without interning it.
        return value;
    }

    private static <T> T findExistingValue(final Entry<T>[] hashTable, final T value, final int hashValue) {
        final int capacity = hashTable.length;

        if (capacity == 0) {
            return null;
        }

        final int idealSlotIndex = getAbsoluteModulus(hashValue, capacity);
        final int slotConflictOffset = getSlotConflictOffset(hashValue, capacity);

        for (int attempt = 0; attempt < capacity; ++attempt) {
            final int index = getAbsoluteModulus(idealSlotIndex + slotConflictOffset * attempt, capacity);
            final Entry<T> existingEntry = hashTable[index];

            if (existingEntry == null) {
                // Our search has encountered a slot that has never been
                // occupied. The entry we're looking for is not present.
                return null;
            }

            if (hashValue != existingEntry.hashValue) {
                continue;
            }

            // Try the refersTo() method first to avoid creating a strong
            // reference to the interned value.
            if (existingEntry.refersTo(value)) {
                // Note: existingEntry cannot be the removedSentinelEntry
                // here because it refers to null, and value cannot be null.
                return value;
            }

            // Note: The existing entry can have a value of null if it is a
            // stale reference or the removedSentinelEntry here. But as
            // above, it is not possible for us to return the
            // removedSentinelEntry here as existingValue will be null and
            // value cannot be null.
            final T existingValue = existingEntry.get();

            if (value.equals(existingValue)) {
                // We have an interned value that is equivalent to the
                // input value. Return the interned value.
                return existingValue;
            }
        }

        return null;
    }

    @SuppressWarnings("unchecked")
    private static <T> Entry<T>[] makeHashTable(final int capacity) {
        return (Entry<T>[]) new Entry<?>[capacity];
    }

    private Entry<T>[] getHashTable() {
        removeStaleEntries();
        return hashTable;
    }

    private void removeStaleEntries() {
        for (Object staleEntry; (staleEntry = staleEntriesQueue.poll()) != null;) {
            @SuppressWarnings("unchecked")
            final Entry<T> removedEntry = (Entry<T>)staleEntry;
            removeEntry(removedEntry);
        }
    }

    private synchronized void removeEntry(final Entry<T> removedEntry) {
        final int capacity = hashTable.length;

        if (capacity == 0) {
            return;
        }

        final int hashValue = removedEntry.hashValue;
        final int idealSlotIndex = getAbsoluteModulus(hashValue, capacity);
        final int slotConflictOffset = getSlotConflictOffset(hashValue, capacity);

        for (int attempt = 0; attempt < capacity; ++attempt) {
            final int index = getAbsoluteModulus(idealSlotIndex + slotConflictOffset * attempt, capacity);
            final Entry<T> entry = hashTable[index];

            if (entry == removedEntry) {
                // We found it. Remove the entry from the hash table by
                // replacing it with a sentinel value. This allows us to
                // distinguish between slots that were once occupied, and slots
                // that were never occupied.
                hashTable[index] = removedSentinelEntry;
                --count;
                return;
            }

            if (entry == null) {
                // Our search has encountered a slot that has never been
                // occupied. The entry we're looking for is not present.
                return;
            }
        }
    }

    private void rehash(final int newCapacity) {
        // Warning: This is expected to be called from a synchronized context.
        final Entry<T>[] oldHashTable = getHashTable();
        final Entry<T>[] newHashTable = makeHashTable(newCapacity);
        count = transferEntries(oldHashTable, newHashTable);
        hashTable = newHashTable;

        if (count < growthThreshold / 2) {
            // Corner case inspired by the JDK WeakHashMap implementation:
            // During transfer, we removed a huge amount of stale entries. So
            // many that we probably shouldn't grow, in fact. Transfer the
            // compacted elements back to the old hash table.
            count = transferEntries(newHashTable, oldHashTable);
            hashTable = oldHashTable;
        } else {
            // We actually needed to grow. Adjust the growth threshold.
            if (newCapacity == MAX_CAPACITY) {
                // We've grown as far as we can. Disable future growth. Once we
                // reach capacity, new entries will no longer be interned.
                growthThreshold = Integer.MAX_VALUE;
            } else {
                // We can grow further in the future. Set the growth threshold
                // based on the new capacity and load factor.
                growthThreshold = (int) (newCapacity * loadFactor);
            }
        }
    }

    private static <T> int transferEntries(final Entry<T>[] sourceHashTable, final Entry<T>[] destinationHashTable) {
        // Warning: This is expected to be called from a synchronized context.
        int validEntryCount = 0;
        final int destinationCapacity = destinationHashTable.length;

        for (int sourceIndex = 0, sourceCapacity = sourceHashTable.length; sourceIndex < sourceCapacity; ++sourceIndex) {
            final Entry<T> entry = sourceHashTable[sourceIndex];
            sourceHashTable[sourceIndex] = null;

            // Only count and transfer valid entries.
            if (entry != null && !entry.refersTo(null)) {
                final int hashValue = entry.hashValue;
                final int idealSlotIndex = getAbsoluteModulus(hashValue, destinationCapacity);
                final int slotConflictOffset = getSlotConflictOffset(hashValue, destinationCapacity);
                assert validEntryCount < destinationCapacity;

                for (int attempt = 0; attempt < destinationCapacity; ++attempt) {
                    final int destinationIndex = getAbsoluteModulus(idealSlotIndex + slotConflictOffset * attempt, destinationCapacity);

                    if (destinationHashTable[destinationIndex] == null) {
                        destinationHashTable[destinationIndex] = entry;
                        ++validEntryCount;
                        break;
                    }
                }
            }
        }

        return validEntryCount;
    }

    private static int getSlotConflictOffset(final int hashValue, final int capacity) {
        return 1 + getAbsoluteModulus(hashValue, capacity - 1);
    }

    private static int getAbsoluteModulus(final int num, final int div) {
        final int modulus = num % div;
        return modulus < 0 ? modulus + div : modulus;
    }

    private static int getNextPrime(int num) {
        if (num < 0) {
            throw new IllegalArgumentException("The argument must be a non-negative number.");
        }

        if (num <= 2) {
            return 2;
        }

        // Ensure odd, so we can skip even numbers.
        if ((num & 1) == 0) {
            ++num;
        }

        while (true) {
            for (int divisor = 3; true; divisor += 2) {
                final int quotient = num / divisor;

                if (quotient < divisor) {
                    return num;
                } else if (num == quotient * divisor) {
                    break;
                }
            }

            num += 2;
        }
    }

    private static int toPrimeCapacity(final int desiredCapacity) {
        if (desiredCapacity < 0) {
            throw new IllegalArgumentException("Capacity cannot be negative.");
        }

        if (desiredCapacity > MAX_CAPACITY) {
            throw new IllegalArgumentException("Capacity must not exceed " + MAX_CAPACITY + ".");
        }

        int index = Arrays.binarySearch(PRIME_CAPACITY_SEQUENCE, desiredCapacity);

        if (index < 0) {
            // Not an exact match. Negate to get the index at which the desired
            // capacity would be inserted in the prime capacity sequence.
            // To be clear, this is also the index referring to the smallest
            // prime in the sequence that can hold the desired capacity.
            index = ~index;
        }

        return PRIME_CAPACITY_SEQUENCE[index];
    }

    private static class Entry<T> extends WeakReference<T> {
        final int hashValue;

        public Entry(final T value, final ReferenceQueue<T> staleEntriesQueue, final int hashValue) {
            super(value, staleEntriesQueue);
            this.hashValue = hashValue;
        }

        @Override
        public boolean equals(final Object other) {
            return (other instanceof Entry<?> otherEntry) && Objects.equals(get(), otherEntry.get());
        }

        @Override
        public int hashCode() {
            return Objects.hash(get());
        }
    }
}
