// Copyright 2020-2023 The Defold Foundation
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
import java.util.Objects;

public final class WeakInterner<T> {
    private static final int MAX_CAPACITY = 1 << 30;
    private static final float DEFAULT_LOAD_FACTOR = 0.75f;

    private Entry<T>[] hashTable;
    private int count;
    private int growthThreshold;
    private final float loadFactor;
    private final ReferenceQueue<T> staleEntriesQueue;

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
        if (initialCapacity < 0) {
            throw new IllegalArgumentException("Initial capacity cannot be negative.");
        }

        if (initialCapacity > MAX_CAPACITY) {
            throw new IllegalArgumentException("Initial capacity must not exceed " + MAX_CAPACITY + ".");
        }

        if (loadFactor <= 0.0f || Float.isNaN(loadFactor)) {
            throw new IllegalArgumentException("Load factor must be a positive number.");
        }

        final int capacity = toPowerOfTwo(initialCapacity);
        this.hashTable = makeHashTable(capacity);
        this.count = 0;
        this.loadFactor = loadFactor;
        this.growthThreshold = (int)(capacity * loadFactor);
        this.staleEntriesQueue = new ReferenceQueue<>();
    }

    /**
     * Returns a canonical representation of an immutable value.
     * Multiple calls to this method with equivalent values will return the
     * previous instance. This can be used to ensure we only have a single
     * canonical representation of a value instead of many duplicate instances.
     * Values are retained as WeakReferences, and storage will be reclaimed once
     * the canonical values are no longer reachable outside the WeakInterner.
     *
     * @param value The immutable value to intern.
     * @return The canonical representation of the supplied value.
     */
    public T intern(final T value) {
        if (value == null) {
            throw new IllegalArgumentException("The interned value cannot be null.");
        }

        // First attempt to get the existing value without locking. If we don't
        // find a match, lock access from other threads while adding it.
        final int hashValue = getHashValue(value);
        final Entry<T>[] hashTable = getHashTable();
        final int bucketIndex = getBucketIndex(hashValue, hashTable.length);
        final Entry<T> firstEntryInBucket = hashTable[bucketIndex];
        final T existingValue = findExistingValue(firstEntryInBucket, value, hashValue);
        return existingValue != null ? existingValue : internSynchronized(value, hashValue);
    }

    private synchronized T internSynchronized(final T value, final int hashValue) {
        // Another thread might have modified the hash table while we were
        // waiting for it to release the lock. Thus, we must get the hash table
        // again and try to find an existing value once more before interning it
        // ourselves.
        final Entry<T>[] hashTable = getHashTable();
        final int bucketIndex = getBucketIndex(hashValue, hashTable.length);
        final Entry<T> firstEntryInBucket = hashTable[bucketIndex];
        final T existingValue = findExistingValue(firstEntryInBucket, value, hashValue);

        if (existingValue != null) {
            // Another thread interned the value while we were waiting.
            return existingValue;
        }

        // We get to intern the value.
        hashTable[bucketIndex] = new Entry<>(value, staleEntriesQueue, hashValue, firstEntryInBucket);

        if (++count >= growthThreshold) {
            rehash(hashTable.length * 2);
        }

        return value;
    }

    private static <T> T findExistingValue(final Entry<T> firstEntryInBucket, final T value, final int hashValue) {
        for (Entry<T> existingEntry = firstEntryInBucket; existingEntry != null; existingEntry = existingEntry.nextEntry) {
            if (hashValue != existingEntry.hashValue) {
                continue;
            }

            if (existingEntry.refersTo(value)) {
                return value;
            }

            final T existingValue = existingEntry.get();

            if (value.equals(existingValue)) {
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
        final int bucketIndex = getBucketIndex(removedEntry.hashValue, hashTable.length);
        Entry<T> previousEntry = null;
        Entry<T> entry = hashTable[bucketIndex];

        while (entry != null) {
            final Entry<T> nextEntry = entry.nextEntry;

            if (entry == removedEntry) {
                // We found it. Remove the entry from the bucket.
                if (previousEntry == null) {
                    hashTable[bucketIndex] = nextEntry;
                } else {
                    previousEntry.nextEntry = nextEntry;
                }

                // It should be safe to clear out references to other objects to
                // help the garbage collector here, since nothing should have a
                // reference to the entry.
                entry.nextEntry = null;
                --count;
                return;
            }

            previousEntry = entry;
            entry = nextEntry;
        }
    }

    private void rehash(final int newCapacity) {
        // Warning: This is expected to be called from a synchronized context.
        final Entry<T>[] oldHashTable = getHashTable();
        final int oldCapacity = oldHashTable.length;

        if (oldCapacity == MAX_CAPACITY) {
            // We've grown as far as we can. Disable future growth and return.
            // Future insertions will just have to share buckets.
            growthThreshold = Integer.MAX_VALUE;
            return;
        }

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
            growthThreshold = (int) (newCapacity * loadFactor);
        }
    }

    private static <T> int transferEntries(final Entry<T>[] sourceHashTable, final Entry<T>[] destinationHashTable) {
        // Warning: This is expected to be called from a synchronized context.
        int validEntryCount = 0;
        final int destinationBucketCount = destinationHashTable.length;

        for (int sourceBucketIndex = 0, sourceBucketCount = sourceHashTable.length; sourceBucketIndex < sourceBucketCount; ++sourceBucketIndex) {
            Entry<T> entry = sourceHashTable[sourceBucketIndex];
            sourceHashTable[sourceBucketIndex] = null;

            while (entry != null) {
                final Entry<T> nextEntry = entry.nextEntry;

                if (entry.refersTo(null)) {
                    // This is a stale entry. Don't count or attempt to transfer
                    // it. We also clear out references to other objects to help
                    // the garbage collector.
                    entry.nextEntry = null;
                } else {
                    // This is a valid entry. Transfer it to the destination.
                    int destinationBucketIndex = getBucketIndex(entry.hashValue, destinationBucketCount);
                    entry.nextEntry = destinationHashTable[destinationBucketIndex];
                    destinationHashTable[destinationBucketIndex] = entry;
                    ++validEntryCount;
                }

                entry = nextEntry;
            }
        }

        return validEntryCount;
    }

    private static int getHashValue(final Object value) {
        // Bitwise trickery that ensures we have a hash of decent-quality
        // necessitated by our use of power-of-two sized hash tables.
        int hashValue = value.hashCode();
        hashValue ^= (hashValue >>> 20) ^ (hashValue >>> 12);
        return hashValue ^ (hashValue >>> 7) ^ (hashValue >>> 4);
    }

    private static int getBucketIndex(final int hashValue, final int bucketCount) {
        return hashValue & (bucketCount - 1);
    }

    private static int toPowerOfTwo(final int num) {
        int powerOfTwo = 1;

        while (powerOfTwo < num) {
            powerOfTwo <<= 1;
        }

        return powerOfTwo;
    }

    private static class Entry<T> extends WeakReference<T> {
        final int hashValue;
        Entry<T> nextEntry;

        public Entry(final T value, final ReferenceQueue<T> staleEntriesQueue, final int hashValue, final Entry<T> nextEntry) {
            super(value, staleEntriesQueue);
            this.hashValue = hashValue;
            this.nextEntry = nextEntry;
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
