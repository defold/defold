package com.defold.util;

import java.io.Serializable;
import java.util.*;

public class LongArrayList extends AbstractList<Long> implements Cloneable, RandomAccess, Serializable {
    // Apparently some JVMs will throw an OutOfMemoryError if you try to
    // allocate an array whose length is close to the signed integer limit.
    private static final int MAX_ARRAY_LENGTH = Integer.MAX_VALUE - 8;

    private static final long[] NO_DATA = new long[0];

    private long[] data;
    private int size;

    // -------------------------------------------------------------------------
    // Constructors.
    // -------------------------------------------------------------------------

    public LongArrayList() {
        data = NO_DATA;
        size = 0;
    }

    public LongArrayList(int initialCapacity) {
        data = new long[initialCapacity];
        size = 0;
    }

    public LongArrayList(long[] src) {
        size = src.length;
        data = Arrays.copyOfRange(src, 0, size);
    }

    public LongArrayList(long[] src, int srcPos, int srcLength) {
        size = srcLength;
        data = Arrays.copyOfRange(src, srcPos, srcPos + srcLength);
    }

    public LongArrayList(Collection<? extends Long> src) {
        size = src.size();
        data = new long[size];
        final Iterator<? extends Long> iterator = src.iterator();

        for (int i = 0; i < size; ++i) {
            data[i] = iterator.next();
        }
    }

    // -------------------------------------------------------------------------
    // Equality.
    // -------------------------------------------------------------------------

    @Override
    public boolean equals(Object value) {
        if (value == null || getClass() != value.getClass()) {
            return false;
        }

        LongArrayList other = (LongArrayList)value;
        return size == other.size && Arrays.equals(data, 0, size, other.data, 0, size);
    }

    @Override
    public int hashCode() {
        int result = 1;

        for (int i = 0; i < size; ++i) {
            result = 31 * result + Long.hashCode(data[i]);
        }

        return result;
    }

    // -------------------------------------------------------------------------
    // ArrayList capabilities.
    // -------------------------------------------------------------------------

    public void ensureCapacity(int requiredCapacity) {
        if (data.length < requiredCapacity) {
            growToCapacity(requiredCapacity);
        }
    }

    public void trimToSize() {
        if (size < data.length) {
            data = size == 0 ? NO_DATA : Arrays.copyOf(data, size);
        }
    }

    // -------------------------------------------------------------------------
    // Primitive operations.
    // -------------------------------------------------------------------------

    public long getLongAt(int index) {
        Objects.checkIndex(index, size);
        return data[index];
    }

    public long setLongAt(int index, long element) {
        Objects.checkIndex(index, size);
        final long old = data[index];
        data[index] = element;
        return old;
    }

    public void addLong(long element) {
        ensureCapacity(size + 1);
        data[size] = element;
        ++size;
    }

    public void addLongAt(int index, long element) {
        Objects.checkIndex(index, size);
        ensureCapacity(size + 1);
        System.arraycopy(data, index, data, index + 1, size - index);
        data[index] = element;
        ++size;
    }

    public boolean addLongs(LongArrayList src) {
        return addLongsAt(0, src.data, 0, src.size);
    }

    public boolean addLongs(Collection<? extends Long> src) {
        return addLongs(src, src.size());
    }

    public boolean addLongs(Iterable<? extends Long> src, int srcLength) {
        if (srcLength == 0) {
            return false;
        }

        ensureCapacity(size + srcLength);
        final Iterator<? extends Long> iterator = src.iterator();

        for (int i = 0; i < srcLength; ++i) {
            data[size + i] = iterator.next();
        }

        size += srcLength;
        return true;
    }

    public boolean addLongs(long[] src) {
        return addLongsAt(0, src, 0, src.length);
    }

    public boolean addLongsAt(int index, LongArrayList src) {
        return addLongsAt(index, src.data, 0, src.size);
    }

    public boolean addLongsAt(int index, LongArrayList src, int srcPos, int srcLength) {
        Objects.checkFromIndexSize(srcPos, srcLength, src.size);
        return addLongsAt(index, src.data, srcPos, srcLength);
    }

    public boolean addLongsAt(int index, Collection<? extends Long> src) {
        return addLongsAt(index, src, src.size());
    }

    public boolean addLongsAt(int index, Iterable<? extends Long> src, int srcLength) {
        if (srcLength == 0) {
            return false;
        }

        Objects.checkIndex(index, size);
        ensureCapacity(size + srcLength);
        System.arraycopy(data, index, data, index + srcLength, size - index);
        final Iterator<? extends Long> iterator = src.iterator();

        for (int i = 0; i < srcLength; ++i) {
            data[index + i] = iterator.next();
        }

        size += srcLength;
        return true;
    }

    public boolean addLongsAt(int index, long[] src) {
        return addLongsAt(index, src, 0, src.length);
    }

    public boolean addLongsAt(int index, long[] src, int srcPos, int srcLength) {
        if (srcLength == 0) {
            return false;
        }

        Objects.checkIndex(index, size);
        ensureCapacity(size + srcLength);
        System.arraycopy(data, index, data, index + srcLength, size - index);
        System.arraycopy(src, srcPos, data, index, srcLength);
        size += srcLength;
        return true;
    }

    public boolean removeLong(long element) {
        int index = indexOfLong(element);

        if (index == -1) {
            return false;
        }

        System.arraycopy(data, index + 1, data, index, size - index - 1);
        --size;
        return true;
    }

    public long removeLongAt(int index) {
        Objects.checkIndex(index, size);
        final long removed = data[index];
        System.arraycopy(data, index + 1, data, index, size - index - 1);
        --size;
        return removed;
    }

    public int indexOfLong(long element) {
        for (int i = 0; i < size; ++i) {
            if (data[i] == element) {
                return i;
            }
        }

        return -1;
    }

    public int lastIndexOfLong(long element) {
        for (int i = size - 1; i >= 0; --i) {
            if (data[i] == element) {
                return i;
            }
        }

        return -1;
    }

    public long[] toLongArray() {
        return Arrays.copyOf(data, size);
    }

    public void sortLongs() {
        Arrays.sort(data, 0, size);
    }

    // -------------------------------------------------------------------------
    // Interface implementations.
    // -------------------------------------------------------------------------

    @Override
    public int size() {
        return size;
    }

    @Override
    public boolean isEmpty() {
        return size == 0;
    }

    @Override
    public Long get(int index) {
        return getLongAt(index);
    }

    @Override
    public Long set(int index, Long element) {
        return setLongAt(index, element);
    }

    @Override
    public boolean add(Long element) {
        addLong(element);
        return true;
    }

    @Override
    public void add(int index, Long element) {
        addLongAt(index, element);
    }

    @Override
    public Long remove(int index) {
        return removeLongAt(index);
    }

    @Override
    public void clear() {
        size = 0;
    }

    @Override
    public int indexOf(Object element) {
        return indexOfLong((long)element);
    }

    @Override
    public int lastIndexOf(Object element) {
        return lastIndexOfLong((long)element);
    }

    @Override
    public boolean contains(Object element) {
        return indexOfLong((long)element) != -1;
    }

    @Override
    public Object[] toArray() {
        final Object[] result = new Object[size];

        for (int i = 0; i < size; ++i) {
            result[i] = data[i];
        }

        return result;
    }

    @Override
    public LongArrayList clone() {
        try {
            LongArrayList clone = (LongArrayList)super.clone();
            clone.size = size;
            clone.data = Arrays.copyOf(data, size);
            return clone;
        } catch (CloneNotSupportedException e) {
            // Cannot happen, since we implement the Cloneable tag interface.
            throw new InternalError(e);
        }
    }

    // -------------------------------------------------------------------------
    // Internals.
    // -------------------------------------------------------------------------

    private void growToCapacity(int requiredCapacity) {
        int newCapacity = Math.max(4, data.length);

        while (newCapacity < requiredCapacity) {
            newCapacity = newCapacity << 1;

            if (newCapacity < 0 || newCapacity > MAX_ARRAY_LENGTH) {
                // Overflow.
                newCapacity = MAX_ARRAY_LENGTH;
            }
        }

        if (newCapacity < requiredCapacity) {
            throw new OutOfMemoryError("Required array length " + requiredCapacity + " is too large");
        }

        data = Arrays.copyOf(data, newCapacity);
    }
}
