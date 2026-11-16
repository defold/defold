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

import clojure.lang.AFn;
import clojure.lang.ASeq;
import clojure.lang.ArrayChunk;
import clojure.lang.ArraySeq;
import clojure.lang.Counted;
import clojure.lang.IChunk;
import clojure.lang.IChunkedSeq;
import clojure.lang.IHashEq;
import clojure.lang.IFn;
import clojure.lang.IPersistentCollection;
import clojure.lang.IPersistentMap;
import clojure.lang.IPersistentSet;
import clojure.lang.IReduceInit;
import clojure.lang.IRecord;
import clojure.lang.ISeq;
import clojure.lang.ITransientMap;
import clojure.lang.ITransientVector;
import clojure.lang.Indexed;
import clojure.lang.Murmur3;
import clojure.lang.Obj;
import clojure.lang.PersistentArrayMap;
import clojure.lang.PersistentHashMap;
import clojure.lang.PersistentHashSet;
import clojure.lang.PersistentList;
import clojure.lang.PersistentVector;
import clojure.lang.RT;
import clojure.lang.Reduced;
import clojure.lang.Sequential;
import clojure.lang.Util;

import java.util.AbstractList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.NoSuchElementException;

/**
 * An ordered collection with stable, monotonically assigned slot IDs.
 * Indexed reads count live values. Editing a slot leaves the other IDs intact.
 */
public final class PkidVector extends AFn implements IPersistentCollection, Indexed, List<Object>,
        IReduceInit, Sequential, IHashEq {
    private static final int BITS = 5;
    private static final int MASK = (1 << BITS) - 1;
    private static final Object REMOVED = new Object();
    private static final short[] UNHASHABLE = {};
    private static final int FILTER_WORDS = Long.SIZE / Short.SIZE;

    private static final Object[] NO_VALUES = {};
    public static final PkidVector EMPTY = new PkidVector(null, NO_VALUES, 0, 0);
    public final long nextPkid;
    private final Node root;
    private final Object[] tail;
    private final int tailMask;

    private PkidVector(Node root, Object[] tail, long nextPkid, int tailMask) {
        this.root = root;
        this.tail = tail;
        this.nextPkid = nextPkid;
        this.tailMask = tailMask;
    }

    private long tailBase() {
        return nextPkid == 0 ? 0 : Node.base(nextPkid - 1, 0);
    }

    @Override
    public int count() {
        return treeCount() + tail.length;
    }

    private int treeCount() {
        return root == null ? 0 : root.count;
    }

    private Object liveValue(int index) {
        int treeCount = treeCount();
        if (index >= treeCount) {
            return tail[index - treeCount];
        }
        return nextPkid == (long) treeCount + tail.length ? root.at(index) : root.nth(index);
    }

    @Override
    public Object nth(int index) {
        if (index < 0 || index >= count()) {
            throw new IndexOutOfBoundsException(index);
        }
        return liveValue(index);
    }

    @Override
    public Object nth(int index, Object notFound) {
        return index < 0 || index >= count() ? notFound : liveValue(index);
    }

    @Override
    public Object invoke(Object index) {
        if (!Util.isInteger(index)) {
            throw new IllegalArgumentException("Key must be integer");
        }
        return nth(((Number) index).intValue());
    }

    @Override
    public PkidVector cons(Object value) {
        long next = Math.incrementExact(nextPkid);
        int bit = Node.bit(nextPkid, 0);
        if (nextPkid != 0 && bit == 1) {
            return new PkidVector(flushTail(tailBase()), new Object[]{value}, next, bit);
        }
        Object[] appended = Arrays.copyOf(tail, tail.length + 1);
        appended[tail.length] = value;
        return new PkidVector(root, appended, next, tailMask | bit);
    }

    private Node flushTail(long base) {
        return tail.length == 0 ? root : Node.appendLeaf(root, new Node(base, 0, tailMask, tail.length, tail));
    }

    @Override
    public PkidVector empty() {
        return EMPTY;
    }

    public PkidVector assocPkids(Object pkids, Object value) {
        return edit(pkids, value);
    }

    public PkidVector dissocPkids(Object pkids) {
        return edit(pkids, REMOVED);
    }

    private PkidVector edit(Object pkids, Object value) {
        if (pkids == null) {
            return this;
        }
        int size = pkids instanceof Counted counted ? counted.count() : -1;
        if (size == 0) {
            return this;
        }
        if (size > 0 && pkids instanceof Indexed indexed) {
            if (size == 1) {
                return withSlot(checkedPkid(indexed.nth(0)), value);
            }
            if (root == null && value == REMOVED) {
                long base = tailBase();
                int removed = 0;
                for (int i = 0; i < size; i++) {
                    long id = checkedPkid(indexed.nth(i));
                    if (base <= id && id < nextPkid) {
                        removed |= Node.bit(id, 0);
                    }
                }
                return withoutTailSlots(null, removed);
            }
        }
        Edit edit = new Edit(Math.min(size, 256));
        if (pkids instanceof IReduceInit reducible) {
            reducible.reduce(edit, null);
        } else {
            Iterator<?> iterator = RT.iter(pkids);
            while (iterator.hasNext()) {
                edit.add(iterator.next());
            }
        }
        if (edit.size == 0) {
            return this;
        }
        if (edit.size == 1) {
            return withSlot(edit.ids[0], value);
        }
        if (!edit.sorted) {
            Arrays.sort(edit.ids, 0, edit.size);
        }
        if (value == REMOVED) {
            long tailBase = tailBase();
            int tailStart = edit.size;
            int removed = 0;
            while (tailStart > 0 && edit.ids[tailStart - 1] >= tailBase) {
                long id = edit.ids[--tailStart];
                if (id < nextPkid) {
                    removed |= Node.bit(id, 0);
                }
            }
            Node updated = root == null ? null : root.remove(edit.ids, 0, tailStart);
            return withoutTailSlots(updated, removed);
        }
        long base = tailBase();
        Node updated = root;
        int i = 0;
        if (root != null) {
            int shift = root.shift();
            while (i < edit.size && edit.ids[i] < base && Node.base(edit.ids[i], shift) == root.base()) {
                i++;
            }
            if (i > 1) {
                updated = root.assocWithin(edit.ids, 0, i, value);
            } else {
                i = 0;
            }
        }
        PkidVector result = updated == root ? this : new PkidVector(updated, tail, nextPkid, tailMask);
        while (i < edit.size && edit.ids[i] < base) {
            result = result.withSlot(edit.ids[i++], value);
        }
        while (i < edit.size) {
            int start = i;
            long page = Node.base(edit.ids[i], 0);
            int changes = 0;
            do {
                changes |= Node.bit(edit.ids[i++], 0);
            } while (i < edit.size && Node.base(edit.ids[i], 0) == page);
            if (i - start == 1) {
                result = result.withSlot(edit.ids[start], value);
            } else {
                result = result.assocTail(page, changes, edit.ids[i - 1], value);
            }
        }
        return result;
    }

    private PkidVector assocTail(long page, int changes, long highest, Object value) {
        long base = tailBase();
        boolean samePage = page == base;
        int mask = samePage ? tailMask : 0;
        Object[] before = samePage ? tail : NO_VALUES;
        Object[] values = Node.assocValues(before, mask, changes, value);
        if (values == before) {
            return this;
        }
        long next = Math.max(nextPkid, Math.incrementExact(highest));
        return new PkidVector(samePage ? root : flushTail(base), values, next, mask | changes);
    }

    private static long checkedPkid(Object value) {
        long pkid = (long) value;
        if (pkid < 0) {
            throw new IllegalArgumentException("pkid must not be negative: " + pkid);
        }
        return pkid;
    }

    private PkidVector withSlot(long pkid, Object value) {
        if (pkid == nextPkid && value != REMOVED) {
            return cons(value);
        }
        long base = tailBase();
        long slotBase = Node.base(pkid, 0);
        if (slotBase < base) {
            Node updated = Node.change(root, pkid, value);
            if (updated == root) {
                return this;
            }
            return new PkidVector(updated, tail, nextPkid, tailMask);
        }
        int bit = Node.bit(pkid, 0);
        if (value == REMOVED) {
            return slotBase == base ? withoutTailSlots(root, bit) : this;
        }
        long next = Math.max(nextPkid, Math.incrementExact(pkid));
        if (slotBase > base) {
            return new PkidVector(flushTail(base), new Object[]{value}, next, bit);
        }
        int index = Node.index(tailMask, bit);
        Object[] updated;
        if ((tailMask & bit) != 0) {
            if (tail[index] == value) {
                return this;
            }
            updated = tail.clone();
        } else {
            updated = Arrays.copyOf(tail, tail.length + 1);
            System.arraycopy(updated, index, updated, index + 1, tail.length - index);
        }
        updated[index] = value;
        return new PkidVector(root, updated, next, tailMask | bit);
    }

    private PkidVector withoutTailSlots(Node updatedRoot, int removed) {
        int remaining = tailMask & ~removed;
        if (remaining == tailMask) {
            return updatedRoot == root ? this : new PkidVector(updatedRoot, tail, nextPkid, tailMask);
        }
        Object[] updated = remaining == 0 ? NO_VALUES : new Object[Integer.bitCount(remaining)];
        int bits = remaining;
        for (int i = 0; i < updated.length; i++) {
            int bit = Integer.lowestOneBit(bits);
            updated[i] = tail[Node.index(tailMask, bit)];
            bits ^= bit;
        }
        return new PkidVector(updatedRoot, updated, nextPkid, remaining);
    }

    private static final class Edit extends AFn {
        private final int capacity;
        long[] ids;
        int size;
        boolean sorted = true;

        Edit(int capacity) {
            this.capacity = Math.max(8, capacity);
        }

        void add(Object id) {
            long pkid = checkedPkid(id);
            if (size == 0) {
                ids = new long[capacity];
            } else {
                sorted &= pkid >= ids[size - 1];
                if (size == ids.length) {
                    ids = Arrays.copyOf(ids, size * 2);
                }
            }
            ids[size++] = pkid;
        }

        @Override
        public Object invoke(Object accumulator, Object id) {
            add(id);
            return accumulator;
        }
    }

    public PersistentVector findPkids(Object value) {
        if (root == null) {
            long base = tailBase();
            int bits = tailMask;
            PersistentVector matches = PersistentVector.EMPTY;
            for (Object stored : tail) {
                if (Util.equiv(value, stored)) {
                    matches = matches.cons(base + Integer.numberOfTrailingZeros(bits));
                }
                bits &= bits - 1;
            }
            return matches;
        }
        SlotMatches result = new SlotMatches();
        root.findPkids(result, value);
        result.scan(tailBase(), tailMask, tail, value);
        return result.persistent();
    }

    private static final class SlotMatches {
        private PersistentVector small = PersistentVector.EMPTY;
        private ITransientVector large;

        void add(long slot) {
            if (large == null) {
                if (small.count() < (1 << BITS)) {
                    small = small.cons(slot);
                    return;
                }
                large = small.asTransient();
            }
            large = (ITransientVector) large.conj(slot);
        }

        void scan(long base, int bits, Object[] values, Object query) {
            for (Object value : values) {
                if (Util.equiv(query, value)) {
                    add(base + Integer.numberOfTrailingZeros(bits));
                }
                bits &= bits - 1;
            }
        }

        PersistentVector persistent() {
            return large == null ? small : (PersistentVector) large.persistent();
        }
    }

    private static boolean indexable(Object value) {
        return value instanceof IRecord || value instanceof Long;
    }

    private static int lookupHash(Object value) {
        int hash = Util.hasheq(value);
        return hash ^ (hash >>> 16);
    }

    private static int filterBit(short hash) {
        return (hash & 0xffff) >>> (Short.SIZE - 6);
    }

    private static boolean mayContain(short[] data, short hash) {
        int bit = filterBit(hash);
        return (data[bit / Short.SIZE] & (1 << (bit % Short.SIZE))) != 0;
    }

    private IPersistentMap findOneRequestedValue(IPersistentSet requested, Object query) {
        short hash = (short) lookupHash(query);
        ITransientMap matches = PersistentHashMap.EMPTY.asTransient();
        matches = root.collectQueryMatches(requested, hash, matches);
        return collectQueryMatches(tailBase(), tailMask, tail, null, requested, hash, matches).persistent();
    }

    private static ITransientMap collectQueryMatches(long base, int bits, Object[] entries,
            short[] hashes, IPersistentSet requested, short queryHash, ITransientMap matches) {
        if (hashes == null) {
            for (Object value : entries) {
                if ((!indexable(value) || (short) lookupHash(value) == queryHash) && requested.contains(value)) {
                    PersistentVector found = (PersistentVector) matches.valAt(value, PersistentVector.EMPTY);
                    matches = matches.assoc(value, found.cons(base + Integer.numberOfTrailingZeros(bits)));
                }
                bits &= bits - 1;
            }
            return matches;
        }
        // Mark the compact array indexes before converting any of them to slot IDs.
        int matchesByIndex = 0;
        for (int i = 0; i < entries.length; i++) {
            if (hashes[FILTER_WORDS + i] == queryHash) {
                matchesByIndex |= 1 << i;
            }
        }
        while (matchesByIndex != 0) {
            int index = Integer.numberOfTrailingZeros(matchesByIndex);
            Object value = entries[index];
            if (requested.contains(value)) {
                int bit = Integer.expand(Integer.lowestOneBit(matchesByIndex), bits);
                PersistentVector found = (PersistentVector) matches.valAt(value, PersistentVector.EMPTY);
                matches = matches.assoc(value, found.cons(base + Integer.numberOfTrailingZeros(bit)));
            }
            matchesByIndex &= matchesByIndex - 1;
        }
        return matches;
    }

    public IPersistentMap findPkidsByValue(IPersistentSet values) {
        if (count() == 0 || values.count() == 0) {
            return PersistentHashMap.EMPTY;
        }
        if (root == null && values.count() <= 4) {
            IPersistentMap matches = PersistentArrayMap.EMPTY;
            int bits = tailMask;
            long base = tailBase();
            for (Object value : tail) {
                if (values.contains(value)) {
                    PersistentVector pkids = (PersistentVector) matches.valAt(value, PersistentVector.EMPTY);
                    matches = matches.assoc(value, pkids.cons(base + Integer.numberOfTrailingZeros(bits)));
                }
                bits &= bits - 1;
            }
            return matches;
        }
        if (values.getClass() == PersistentHashSet.class && values.count() == 1 && count() >= 1024) {
            Object query = values.seq().first();
            if (indexable(query)) {
                return findOneRequestedValue(values, query);
            }
        }
        long[] filter = null;
        if (values instanceof PersistentHashSet && count() >= 4096 && values.count() <= count() / 32) {
            filter = lookupFilter(values);
        }
        ITransientMap result = PersistentHashMap.EMPTY.asTransient();
        if (root != null) {
            result = root.findPkidsByValue(values, filter, result);
        }
        return collectMatches(tailBase(), tailMask, tail, values, filter, null, result).persistent();
    }

    private static long[] lookupFilter(IPersistentSet requested) {
        int length = Math.min(1024, Integer.highestOneBit(Math.max(2, requested.count() / 4) - 1) << 1);
        long[] filter = new long[length];
        for (var remaining = requested.seq(); remaining != null; remaining = remaining.next()) {
            Object value = remaining.first();
            if (!indexable(value)) {
                return null;
            }
            int hash = lookupHash(value);
            filter[(hash >>> 6) & (length - 1)] |= 1L << hash;
        }
        return filter;
    }

    private static boolean mayMatch(long[] filter, Object value) {
        if (filter == null || !indexable(value)) {
            return true;
        }
        int hash = lookupHash(value);
        return (filter[(hash >>> 6) & (filter.length - 1)] & (1L << hash)) != 0;
    }

    private static ITransientMap collectMatches(long base, int bits, Object[] entries,
            IPersistentSet values, long[] filter, short[] hashes, ITransientMap result) {
        boolean dense = bits == -1;
        for (int index = 0; index < entries.length; index++) {
            Object value = entries[index];
            boolean possible;
            if (hashes == null) {
                possible = mayMatch(filter, value);
            } else {
                int hash = hashes[FILTER_WORDS + index] & 0xffff;
                possible = (filter[(hash >>> 6) & (filter.length - 1)] & (1L << hash)) != 0;
            }
            if (possible && values.contains(value)) {
                long pkid = base + (dense ? index : Integer.numberOfTrailingZeros(bits));
                PersistentVector matches = (PersistentVector) result.valAt(value, PersistentVector.EMPTY);
                result = result.assoc(value, matches.cons(pkid));
            }
            bits &= bits - 1;
        }
        return result;
    }

    @Override
    public Iterator<Object> iterator() {
        return new SlotIterator(this);
    }

    private static final class SlotIterator implements Iterator<Object> {
        private final PkidVector table;
        private final int end;
        private final Node[] path;
        private long positions;
        private Object[] values;
        private int depth;
        private int index;
        private int pageStart;

        SlotIterator(PkidVector table) {
            this.table = table;
            end = table.count();
            Node root = table.root;
            int levels = root == null || table.nextPkid == end ? 0 : root.shift() / BITS;
            path = levels == 0 ? null : new Node[levels];
            depth = -1;
            if (path != null) {
                descend(root);
            } else {
                values = root == null ? table.tail : root.firstLeaf().entries;
            }
        }

        private void descend(Node node) {
            while (node.shift() != 0) {
                path[++depth] = node;
                positions &= (1L << (depth * BITS)) - 1;
                node = (Node) node.entries[0];
            }
            values = node.entries;
        }

        private void advance() {
            if (path == null) {
                values = index < table.treeCount() ? table.root.denseEntries(index) : table.tail;
                return;
            }
            while (depth >= 0) {
                int shift = depth * BITS;
                int position = ((int) (positions >>> shift) & MASK) + 1;
                Object[] children = path[depth].entries;
                if (position < children.length) {
                    positions = (positions & ((1L << shift) - 1)) | ((long) position << shift);
                    descend((Node) children[position]);
                    return;
                }
                depth--;
            }
            values = table.tail;
        }

        @Override
        public boolean hasNext() {
            return index < end;
        }

        @Override
        public Object next() {
            if (index == end) {
                throw new NoSuchElementException();
            }
            if (index - pageStart == values.length) {
                advance();
                pageStart = index;
            }
            return values[index++ - pageStart];
        }
    }

    @Override
    public ISeq seq() {
        return root == null ? ArraySeq.create(tail) : new SlotSeq(null, root.firstLeaf(), 0, count());
    }

    private final class SlotSeq extends ASeq implements IChunkedSeq, Counted, IReduceInit {
        private final Node leaf;
        private final int offset;
        private final int remaining;

        SlotSeq(IPersistentMap metadata, Node leaf, int offset, int remaining) {
            super(metadata);
            this.leaf = leaf;
            this.offset = offset;
            this.remaining = remaining;
        }

        @Override
        public Obj withMeta(IPersistentMap metadata) {
            return new SlotSeq(metadata, leaf, offset, remaining);
        }

        @Override
        public int count() {
            return remaining;
        }

        @Override
        public Object first() {
            return leaf.entries[offset];
        }

        @Override
        public ISeq next() {
            if (offset + 1 < leaf.entries.length) {
                return new SlotSeq(null, leaf, offset + 1, remaining - 1);
            }
            return chunkedNext();
        }

        @Override
        public IChunk chunkedFirst() {
            return new ArrayChunk(leaf.entries, offset);
        }

        @Override
        public ISeq chunkedNext() {
            int size = remaining - (leaf.entries.length - offset);
            if (size == 0) {
                return null;
            }
            Node next = root.leafAtOrAfter(leaf.base() + (1 << BITS));
            return next == null ? ArraySeq.create(tail) : new SlotSeq(null, next, 0, size);
        }

        @Override
        public ISeq chunkedMore() {
            ISeq next = chunkedNext();
            return next == null ? PersistentList.EMPTY : next;
        }

        @Override
        public Object reduce(IFn f, Object initial) {
            Node current = leaf;
            Object result = initial;
            for (int i = offset; i < current.entries.length; i++) {
                result = f.invoke(result, current.entries[i]);
                if (result instanceof Reduced reduced) {
                    return reduced.deref();
                }
            }
            while ((current = root.leafAtOrAfter(current.base() + (1 << BITS))) != null) {
                result = Node.reduceValues(current.entries, f, result);
                if (result instanceof Reduced reduced) {
                    return reduced.deref();
                }
            }
            result = Node.reduceValues(tail, f, result);
            return result instanceof Reduced reduced ? reduced.deref() : result;
        }
    }

    @Override
    public Object reduce(IFn f, Object initial) {
        if (count() == 0) {
            return initial;
        }
        Object result = initial;
        if (root != null) {
            result = root.reduce(f, initial);
            if (result instanceof Reduced reduced) {
                return reduced.deref();
            }
        }
        result = Node.reduceValues(tail, f, result);
        return result instanceof Reduced reduced ? reduced.deref() : result;
    }

    @Override
    public boolean equiv(Object other) {
        if (this == other) {
            return true;
        }
        if (!(other instanceof Sequential || other instanceof List<?>)) {
            return false;
        }
        if (other instanceof Counted counted && count() != counted.count()) {
            return false;
        }
        Iterator<?> values = RT.iter(other);
        for (Object value : this) {
            if (!values.hasNext() || !Util.equiv(value, values.next())) {
                return false;
            }
        }
        return !values.hasNext();
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) {
            return true;
        }
        if (!(other instanceof Sequential || other instanceof List<?>)) {
            return false;
        }
        if (other instanceof Counted counted && count() != counted.count()) {
            return false;
        }
        Iterator<?> values = RT.iter(other);
        for (Object value : this) {
            if (!values.hasNext() || !Util.equals(value, values.next())) {
                return false;
            }
        }
        return !values.hasNext();
    }

    @Override
    public int hashCode() {
        int hash = 1;
        for (Object value : this) {
            hash = 31 * hash + Util.hash(value);
        }
        return hash;
    }

    @Override
    public int hasheq() {
        return Murmur3.hashOrdered(this);
    }

    @Override
    public String toString() {
        return RT.printString(seq());
    }

    @Override
    public int size() {
        return count();
    }

    @Override
    public boolean isEmpty() {
        return count() == 0;
    }

    @Override
    public Object get(int index) {
        return nth(index);
    }

    @Override
    public boolean contains(Object value) {
        return indexOf(value) >= 0;
    }

    @Override
    public boolean containsAll(Collection<?> values) {
        for (Object value : values) {
            if (!contains(value)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public int indexOf(Object value) {
        int index = 0;
        for (Object entry : this) {
            if (Util.equals(value, entry)) {
                return index;
            }
            index++;
        }
        return -1;
    }

    @Override
    public int lastIndexOf(Object value) {
        return listView().lastIndexOf(value);
    }

    @Override
    public Object[] toArray() {
        return listView().toArray();
    }

    @Override
    public <T> T[] toArray(T[] values) {
        return listView().toArray(values);
    }

    @Override
    public ListIterator<Object> listIterator() {
        return listIterator(0);
    }

    @Override
    public ListIterator<Object> listIterator(int index) {
        return listView().listIterator(index);
    }

    @Override
    public List<Object> subList(int fromIndex, int toIndex) {
        return listView().subList(fromIndex, toIndex);
    }

    private List<Object> listView() {
        return new AbstractList<>() {
            @Override
            public int size() {
                return PkidVector.this.count();
            }

            @Override
            public Object get(int index) {
                return nth(index);
            }

            @Override
            public Iterator<Object> iterator() {
                return PkidVector.this.iterator();
            }
        };
    }

    @Override
    public Object set(int index, Object value) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean add(Object value) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void add(int index, Object value) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean addAll(Collection<?> values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean addAll(int index, Collection<?> values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Object remove(int index) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean remove(Object value) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean removeAll(Collection<?> values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean retainAll(Collection<?> values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void clear() {
        throw new UnsupportedOperationException();
    }

    /**
     * Leaves hold live values, and branches hold child nodes. The bitmap maps
     * slot digits to packed array indexes. Counts make live indexes independent
     * of gaps, and base lets a branch omit levels that have only one child.
     */
    private static class Node {
        final long prefix;
        final int mask;
        final int count;
        final Object[] entries;
        // Leaves cache a presence filter followed by a hash for each live value.
        private volatile short[] valueHashes;

        Node(long base, int shift, int mask, int count, Object[] entries) {
            prefix = base | shift / BITS;
            this.mask = mask;
            this.count = count;
            this.entries = entries;
        }

        private static final class Branch extends Node {
            private volatile int[] countSamples;

            Branch(long base, int shift, int mask, int count, Object[] entries) {
                super(base, shift, mask, count, entries);
            }
        }

        private static Node make(long base, int shift, int mask, int count, Object[] entries) {
            return shift == 0 ? new Node(base, 0, mask, count, entries)
                    : new Branch(base, shift, mask, count, entries);
        }

        private long base() {
            return prefix & ~((long) MASK);
        }

        private int shift() {
            return ((int) prefix & MASK) * BITS;
        }

        private static long base(long pkid, int shift) {
            return pkid & (-1L << shift << BITS);
        }

        private static int bit(long pkid, int shift) {
            return 1 << ((int) (pkid >>> shift) & MASK);
        }

        private static int index(int mask, int bit) {
            return Integer.bitCount(mask & (bit - 1));
        }

        private static Node singleton(long pkid, Object value) {
            return new Node(base(pkid, 0), 0, bit(pkid, 0), 1, new Object[]{value});
        }

        static Node appendLeaf(Node node, Node leaf) {
            if (node == null) {
                return leaf;
            }
            if (base(leaf.base(), node.shift()) != node.base()) {
                return join(node, leaf);
            }
            int bit = bit(leaf.base(), node.shift());
            int last = node.entries.length - 1;
            if ((node.mask & bit) == 0) {
                return node.replace(last + 1, bit, REMOVED, leaf, 0, leaf.count);
            }
            Node prior = (Node) node.entries[last];
            Node updated = appendLeaf(prior, leaf);
            return node.replace(last, bit, prior, updated, prior.count, updated.count);
        }

        private static Node join(Node node, Node other) {
            int shift = (63 - Long.numberOfLeadingZeros(node.base() ^ other.base())) / BITS * BITS;
            int mask = bit(node.base(), shift) | bit(other.base(), shift);
            Object[] children = node.base() < other.base() ? new Object[]{node, other} : new Object[]{other, node};
            return new Branch(base(node.base(), shift), shift, mask, node.count + other.count, children);
        }

        Node remove(long[] ids, int start, int end) {
            if (shift() == 0) {
                int removed = 0;
                for (int i = start; i < end; i++) {
                    long id = ids[i];
                    if (base(id, 0) == base()) {
                        removed |= bit(id, 0);
                    }
                }
                int remaining = mask & ~removed;
                if (remaining == mask) {
                    return this;
                }
                if (remaining == 0) {
                    return null;
                }
                Object[] values = new Object[Integer.bitCount(remaining)];
                int bits = remaining;
                for (int i = 0; i < values.length; i++) {
                    int bit = Integer.lowestOneBit(bits);
                    values[i] = entries[index(mask, bit)];
                    bits ^= bit;
                }
                return new Node(base(), 0, remaining, values.length, values);
            }

            int remaining = mask;
            int newCount = count;
            Object[] children = null;
            for (int i = 0; i < entries.length && start < end; i++) {
                Node child = (Node) entries[i];
                while (start < end && ids[start] < child.base()) {
                    start++;
                }
                int childEnd = start;
                while (childEnd < end && base(ids[childEnd], child.shift()) == child.base()) {
                    childEnd++;
                }
                if (start == childEnd) {
                    continue;
                }
                Node updated = child.remove(ids, start, childEnd);
                start = childEnd;
                if (updated != child) {
                    if (children == null) {
                        children = entries.clone();
                    }
                    children[i] = updated;
                    newCount -= child.count;
                    if (updated == null) {
                        remaining ^= bit(child.base(), shift());
                    } else {
                        newCount += updated.count;
                    }
                }
            }
            if (children == null) {
                return this;
            }
            int childCount = Integer.bitCount(remaining);
            if (childCount == 0) {
                return null;
            }
            if (childCount < children.length) {
                int kept = 0;
                for (Object child : children) {
                    if (child != null) {
                        children[kept++] = child;
                    }
                }
                if (childCount == 1) {
                    return (Node) children[0];
                }
                children = Arrays.copyOf(children, childCount);
            }
            return make(base(), shift(), remaining, newCount, children);
        }

        // All ids belong to this node's prefix.
        Node assocWithin(long[] ids, int start, int end, Object value) {
            int shift = shift();
            if (shift == 0) {
                int changes = 0;
                for (int i = start; i < end; i++) {
                    changes |= bit(ids[i], 0);
                }
                Object[] values = assocValues(entries, mask, changes, value);
                return values == entries ? this : new Node(base(), 0, mask | changes, values.length, values);
            }
            Object[] copy = null;
            Node result = this;
            int newCount = count;
            for (int i = start; i < end;) {
                long id = ids[i];
                int bit = bit(id, shift);
                int index = index(mask, bit);
                Node child = (mask & bit) == 0 ? null : (Node) entries[index];
                if (child == null || base(id, child.shift()) != child.base()) {
                    if (copy != null) {
                        result = make(base(), shift, mask, newCount, copy);
                    }
                    for (; i < end; i++) {
                        result = change(result, ids[i], value);
                    }
                    return result;
                }
                int childEnd = i + 1;
                while (childEnd < end && base(ids[childEnd], child.shift()) == child.base()) {
                    childEnd++;
                }
                Node updated = child.assocWithin(ids, i, childEnd, value);
                if (updated != child) {
                    if (copy == null) {
                        copy = entries.clone();
                    }
                    copy[index] = updated;
                    newCount += updated.count - child.count;
                }
                i = childEnd;
            }
            return copy == null ? this : make(base(), shift, mask, newCount, copy);
        }

        static Object[] assocValues(Object[] values, int mask, int changes, Object value) {
            int live = mask | changes;
            if (live == mask) {
                Object[] copy = null;
                while (changes != 0) {
                    int bit = Integer.lowestOneBit(changes);
                    int index = index(mask, bit);
                    if (values[index] != value) {
                        if (copy == null) {
                            copy = values.clone();
                        }
                        copy[index] = value;
                    }
                    changes ^= bit;
                }
                return copy == null ? values : copy;
            }
            Object[] copy = new Object[Integer.bitCount(live)];
            for (int i = 0; i < copy.length; i++) {
                int bit = Integer.lowestOneBit(live);
                copy[i] = (changes & bit) != 0 ? value : values[index(mask, bit)];
                live ^= bit;
            }
            return copy;
        }

        static Node change(Node node, long pkid, Object value) {
            if (node == null) {
                return value == REMOVED ? null : singleton(pkid, value);
            }
            if (base(pkid, node.shift()) != node.base()) {
                if (value == REMOVED) {
                    return node;
                }
                return join(node, singleton(pkid, value));
            }

            int bit = bit(pkid, node.shift());
            int index = index(node.mask, bit);
            boolean present = (node.mask & bit) != 0;
            Object old = present ? node.entries[index] : REMOVED;
            if (node.shift() == 0) {
                return node.replace(index, bit, old, value, present ? 1 : 0, value == REMOVED ? 0 : 1);
            }

            Node before = present ? (Node) old : null;
            Node after = change(before, pkid, value);
            if (before == after) {
                return node;
            }
            return node.replace(index, bit, old, after == null ? REMOVED : after,
                    before == null ? 0 : before.count, after == null ? 0 : after.count);
        }

        private Node replace(int index, int bit, Object old, Object value, int before, int after) {
            if (old == value) {
                return this;
            }
            int newCount = count + after - before;
            if (newCount == 0) {
                return null;
            }
            if (value == REMOVED && shift() != 0 && entries.length == 2) {
                return (Node) entries[1 - index];
            }
            if (value == REMOVED) {
                Object[] newEntries = new Object[entries.length - 1];
                System.arraycopy(entries, 0, newEntries, 0, index);
                System.arraycopy(entries, index + 1, newEntries, index, newEntries.length - index);
                return make(base(), shift(), mask ^ bit, newCount, newEntries);
            }
            if (old == REMOVED) {
                Object[] newEntries = Arrays.copyOf(entries, entries.length + 1);
                System.arraycopy(newEntries, index, newEntries, index + 1, entries.length - index);
                newEntries[index] = value;
                return make(base(), shift(), mask | bit, newCount, newEntries);
            }
            Object[] newEntries = entries.clone();
            newEntries[index] = value;
            return make(base(), shift(), mask, newCount, newEntries);
        }

        Object at(long pkid) {
            return denseEntries(pkid)[(int) pkid & MASK];
        }

        Object[] denseEntries(long pkid) {
            Node node = this;
            int shift;
            while ((shift = node.shift()) != 0) {
                int digit = (int) (pkid >>> shift) & MASK;
                node = (Node) node.entries[digit];
            }
            return node.entries;
        }

        Object nth(int index) {
            Node node = this;
            int shift;
            while ((shift = node.shift()) != 0) {
                if (shift < 30 && node.count == (1L << (shift + BITS))) {
                    return node.at(node.base() + index);
                }
                int childIndex = 0;
                if (node.entries.length > 8) {
                    Branch branch = (Branch) node;
                    int[] samples = branch.countSamples;
                    if (samples == null) {
                        samples = branch.countSamples = node.sampleCounts();
                    }
                    while (childIndex < samples.length && index >= samples[childIndex]) {
                        childIndex++;
                    }
                    if (childIndex > 0) {
                        index -= samples[childIndex - 1];
                    }
                    childIndex *= 8;
                }
                for (; childIndex < node.entries.length; childIndex++) {
                    Node child = (Node) node.entries[childIndex];
                    if (index < child.count) {
                        node = child;
                        break;
                    }
                    index -= child.count;
                }
            }
            return node.entries[index];
        }

        private int[] sampleCounts() {
            int[] samples = new int[(entries.length - 1) / 8];
            int count = 0;
            int child = 0;
            for (int i = 0; i < samples.length; i++) {
                int end = child + 8;
                while (child < end) {
                    count += ((Node) entries[child++]).count;
                }
                samples[i] = count;
            }
            return samples;
        }

        void findPkids(SlotMatches result, Object query) {
            if (shift() == 0) {
                result.scan(base(), mask, entries, query);
                return;
            }
            for (Object entry : entries) {
                ((Node) entry).findPkids(result, query);
            }
        }

        ITransientMap findPkidsByValue(IPersistentSet values, long[] filter, ITransientMap result) {
            if (shift() == 0) {
                short[] hashes = filter == null ? null : valueHashes();
                return collectMatches(base(), mask, entries, values, filter, hashes, result);
            }
            for (Object entry : entries) {
                result = ((Node) entry).findPkidsByValue(values, filter, result);
            }
            return result;
        }

        ITransientMap collectQueryMatches(IPersistentSet requested, short hash, ITransientMap matches) {
            if (shift() == 0) {
                short[] hashes = valueHashes();
                return hashes == null || mayContain(hashes, hash)
                        ? PkidVector.collectQueryMatches(base(), mask, entries, hashes, requested, hash, matches)
                        : matches;
            }
            for (Object child : entries) {
                matches = ((Node) child).collectQueryMatches(requested, hash, matches);
            }
            return matches;
        }

        private short[] valueHashes() {
            short[] hashes = valueHashes;
            if (hashes == null) {
                hashes = new short[FILTER_WORDS + entries.length];
                for (int i = 0; i < entries.length; i++) {
                    Object value = entries[i];
                    if (!indexable(value)) {
                        valueHashes = UNHASHABLE;
                        return null;
                    }
                    short hash = (short) lookupHash(value);
                    int bit = filterBit(hash);
                    hashes[bit / Short.SIZE] |= (short) (1 << (bit % Short.SIZE));
                    hashes[FILTER_WORDS + i] = hash;
                }
                valueHashes = hashes;
            }
            return hashes == UNHASHABLE ? null : hashes;
        }

        Object reduce(IFn f, Object result) {
            if (shift() == 0) {
                return reduceValues(entries, f, result);
            }
            for (Object entry : entries) {
                result = ((Node) entry).reduce(f, result);
                if (result instanceof Reduced) {
                    break;
                }
            }
            return result;
        }

        private static Object reduceValues(Object[] values, IFn f, Object result) {
            for (Object value : values) {
                result = f.invoke(result, value);
                if (result instanceof Reduced) {
                    break;
                }
            }
            return result;
        }

        private Node firstLeaf() {
            Node node = this;
            while (node.shift() != 0) {
                node = (Node) node.entries[0];
            }
            return node;
        }

        private Node leafAtOrAfter(long pkid) {
            if (pkid <= base()) {
                return firstLeaf();
            }
            int shift = shift();
            if (base(pkid, shift) != base()) {
                return null;
            }
            if (shift == 0) {
                return this;
            }
            int bit = bit(pkid, shift);
            int index = index(mask, bit);
            if ((mask & bit) != 0) {
                Node leaf = ((Node) entries[index++]).leafAtOrAfter(pkid);
                if (leaf != null) {
                    return leaf;
                }
            }
            return index < entries.length ? ((Node) entries[index]).firstLeaf() : null;
        }

    }
}
