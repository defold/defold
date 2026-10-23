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
import clojure.lang.APersistentMap;
import clojure.lang.BigInt;
import clojure.lang.IDeref;
import clojure.lang.IEditableCollection;
import clojure.lang.IFn;
import clojure.lang.IKVReduce;
import clojure.lang.IMapEntry;
import clojure.lang.IMapIterable;
import clojure.lang.IObj;
import clojure.lang.IPersistentMap;
import clojure.lang.IPersistentVector;
import clojure.lang.IReduce;
import clojure.lang.ISeq;
import clojure.lang.ITransientAssociative2;
import clojure.lang.ITransientMap;
import clojure.lang.IteratorSeq;
import clojure.lang.MapEntry;
import clojure.lang.RT;
import clojure.lang.Reversible;

import java.io.Serializable;
import java.math.BigInteger;
import java.util.AbstractCollection;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * A persistent long-to-object map with copy-on-write transient editing.
 *
 * <p>Keys are integers in the signed 64-bit range; object APIs also accept their
 * Integer, Short, Byte, BigInt and BigInteger representations. Other keys are
 * absent on lookup/removal and rejected on association. Stored keys are exposed
 * as Longs. Values may be null. Iteration follows signed ascending key order.
 *
 * <p>A compressed 64-way radix trie indexes 64-key pages. Singletons are inline,
 * sparse pages are packed behind a bitmap, and dense pages use direct indexing.
 * Transients copy shared nodes once and then edit their owned nodes in place.
 * Readers of persistent snapshots require no synchronization. A transient must
 * be used by only one thread at a time and cannot be used after persistent().
 * Java serialization requires serializable values and metadata.
 */
@SuppressWarnings("serial") // Values and metadata need not be serializable during ordinary use.
public final class LongMap extends APersistentMap
        implements IObj, IEditableCollection, IMapIterable, IReduce, IKVReduce, Reversible {
    private static final long serialVersionUID = 2L;
    private static final Object NOT_FOUND = new Object();
    public static final LongMap EMPTY = new LongMap(null, 0, null);

    private final Node root;
    private final int count;
    private final IPersistentMap meta;

    private LongMap(Node root, int count, IPersistentMap meta) {
        this.root = root;
        this.count = count;
        this.meta = meta;
    }

    private static boolean isKey(Object key) {
        if (key instanceof Long || key instanceof Integer || key instanceof Short || key instanceof Byte) {
            return true;
        }
        if (key instanceof BigInt bigInt) {
            return bigInt.bipart == null;
        }
        return key instanceof BigInteger bigInteger && bigInteger.bitLength() < 64;
    }

    private static long checkedKey(Object key) {
        if (!isKey(key)) {
            throw new IllegalArgumentException("LongMap keys must be integers in the signed 64-bit range: " + key);
        }
        return ((Number) key).longValue();
    }

    private static Object find(Node node, long key, Object notFound) {
        // Flipping the sign bit makes unsigned trie order equal signed key order.
        long pageKey = (key ^ Long.MIN_VALUE) >>> 6;
        while (node instanceof Branch branch) {
            node = branch.child((int) (pageKey >>> branch.shift) & 63);
        }
        if (node == null || node.key != pageKey) {
            return notFound;
        }
        Page page = (Page) node;
        int slot = (int) key & 63;
        return (page.mask & (1L << slot)) == 0 ? notFound : page.valueAt(slot);
    }

    public Object get(long key) {
        return find(root, key, null);
    }

    public Object get(long key, Object notFound) {
        return find(root, key, notFound);
    }

    public boolean containsKey(long key) {
        return find(root, key, NOT_FOUND) != NOT_FOUND;
    }

    public LongMap assoc(long key, Object value) {
        int[] delta = new int[1];
        Node updated = persistentInsert(root, (key ^ Long.MIN_VALUE) >>> 6, (int) key & 63, value, delta);
        return updated == root ? this : new LongMap(updated, count + delta[0], meta);
    }

    public LongMap without(long key) {
        Node updated = persistentRemove(root, (key ^ Long.MIN_VALUE) >>> 6, (int) key & 63);
        return updated == root ? this : updated == null ? empty() : new LongMap(updated, count - 1, meta);
    }

    /** Like Clojure update: invoke once with the old value, or null for absence. */
    public LongMap update(long key, IFn function) {
        return update(key, function, false);
    }

    /** Update an existing entry, including an entry whose value is null. */
    public LongMap updateIfPresent(long key, IFn function) {
        return update(key, function, true);
    }

    private LongMap update(long key, IFn function, boolean presentOnly) {
        java.util.Objects.requireNonNull(function);
        int[] delta = new int[1];
        Node updated = persistentUpdate(root, (key ^ Long.MIN_VALUE) >>> 6,
                (int) key & 63, function, presentOnly, delta);
        return updated == root ? this : new LongMap(updated, count + delta[0], meta);
    }

    private static Node persistentUpdate(Node node, long key, int slot, IFn function,
                                         boolean presentOnly, int[] delta) {
        if (node instanceof Branch branch && branch.matches(key)) {
            int index = (int) (key >>> branch.shift) & 63;
            Node old = branch.child(index);
            Node child = persistentUpdate(old, key, slot, function, presentOnly, delta);
            if (child == old) {
                return branch;
            }
            Branch result = branch.copyPersistent(index);
            result.setChild(index, child);
            return result;
        }
        Page page = node instanceof Page candidate && candidate.key == key ? candidate : null;
        boolean present = page != null && (page.mask & (1L << slot)) != 0;
        if (!present && presentOnly) {
            return node;
        }
        Object value = function.invoke(page != null && present ? page.valueAt(slot) : null);
        // At this point we have reached the leaf or discovered a missing prefix.
        // Insertion below cannot descend an existing branch a second time.
        return persistentInsert(node, key, slot, value, delta);
    }

    private static Branch joinPersistent(Node left, Node right) {
        int shift = ((63 - Long.numberOfLeadingZeros(left.key ^ right.key)) / 6) * 6;
        int leftSlot = (int) (left.key >>> shift) & 63;
        int rightSlot = (int) (right.key >>> shift) & 63;
        return new Branch(left.key, null, shift, (1L << leftSlot) | (1L << rightSlot),
                leftSlot < rightSlot ? new Node[] {left, right} : new Node[] {right, left});
    }

    private static Node persistentInsert(Node node, long key, int slot, Object value, int[] delta) {
        if (node == null) {
            delta[0] = 1;
            return new Page(key, null, 1L << slot, value, null);
        }
        if (node instanceof Branch branch) {
            if (!branch.matches(key)) {
                return joinPersistent(branch, persistentInsert(null, key, slot, value, delta));
            }
            int index = (int) (key >>> branch.shift) & 63;
            Node old = branch.child(index);
            Node child = persistentInsert(old, key, slot, value, delta);
            if (child == old) {
                return branch;
            }
            Branch result = branch.copyPersistent(index);
            result.setChild(index, child);
            return result;
        }
        Page page = (Page) node;
        if (page.key != key) {
            return joinPersistent(page, persistentInsert(null, key, slot, value, delta));
        }
        boolean present = (page.mask & (1L << slot)) != 0;
        if (present && page.valueAt(slot) == value) {
            return page;
        }
        if (!present) {
            delta[0] = 1;
        }
        Page result = new Page(page.key, null, page.mask, page.single, page.values == null ? null : page.values.clone());
        result.set(slot, value);
        return result;
    }

    private static Node persistentRemove(Node node, long key, int slot) {
        if (node instanceof Branch branch) {
            if (!branch.matches(key)) {
                return branch;
            }
            int index = (int) (key >>> branch.shift) & 63;
            Node old = branch.child(index);
            Node child = persistentRemove(old, key, slot);
            if (child == old) {
                return branch;
            }
            if (child == null && Long.bitCount(branch.mask) == 2) {
                return branch.child(Long.numberOfTrailingZeros(branch.mask & ~(1L << index)));
            }
            if (child != null) {
                Branch result = branch.copyPersistent(index);
                result.replaceChild(index, child);
                return result;
            }
            return branch.afterRemoval(index, null, null);
        }
        if (node == null || node.key != key || (((Page) node).mask & (1L << slot)) == 0) {
            return node;
        }
        return ((Page) node).removed(slot, null);
    }

    @Override
    public Object valAt(Object key) {
        return valAt(key, null);
    }

    @Override
    public Object valAt(Object key, Object notFound) {
        return isKey(key) ? find(root, ((Number) key).longValue(), notFound) : notFound;
    }

    @Override
    public boolean containsKey(Object key) {
        return valAt(key, NOT_FOUND) != NOT_FOUND;
    }

    @Override
    public IMapEntry entryAt(Object key) {
        Object value = valAt(key, NOT_FOUND);
        return value == NOT_FOUND ? null : MapEntry.create(((Number) key).longValue(), value);
    }

    @Override
    public LongMap assoc(Object key, Object value) {
        return assoc(checkedKey(key), value);
    }

    @Override
    public LongMap assocEx(Object key, Object value) {
        long id = checkedKey(key);
        if (containsKey(id)) {
            throw new IllegalArgumentException("Key already present: " + key);
        }
        return assoc(id, value);
    }

    @Override
    public LongMap without(Object key) {
        return isKey(key) ? without(((Number) key).longValue()) : this;
    }

    @Override
    public int count() {
        return count;
    }

    @Override
    public IPersistentMap meta() {
        return meta;
    }

    @Override
    public LongMap withMeta(IPersistentMap meta) {
        return this.meta == meta ? this : new LongMap(root, count, meta);
    }

    @Override
    public LongMap empty() {
        return EMPTY.withMeta(meta);
    }

    @Override
    public Transient asTransient() {
        return new Transient(this);
    }

    @Override
    public Iterator<Object> iterator() {
        return new Cursor(root, 0, false);
    }

    @Override
    public Iterator<Object> keyIterator() {
        return new Cursor(root, 1, false);
    }

    @Override
    public Iterator<Object> valIterator() {
        return new Cursor(root, 2, false);
    }

    @Override
    public Collection<Object> values() {
        // APersistentMap's view traverses MapEntry objects. Rendering needs
        // only values, so use the existing iterator without entries or keys.
        return new AbstractCollection<>() {
            @Override
            public Iterator<Object> iterator() {
                return valIterator();
            }

            @Override
            public int size() {
                return count;
            }
        };
    }

    @Override
    public ISeq seq() {
        return IteratorSeq.create(iterator());
    }

    @Override
    public ISeq rseq() {
        return IteratorSeq.create(new Cursor(root, 0, true));
    }

    private static Object reduceEntries(Node node, IFn function, Object result) {
        if (node instanceof Branch branch) {
            for (long bits = branch.mask; bits != 0; bits &= bits - 1) {
                result = reduceEntries(branch.child(Long.numberOfTrailingZeros(bits)), function, result);
                if (RT.isReduced(result)) {
                    return result;
                }
            }
        } else if (node != null) {
            Page page = (Page) node;
            for (long bits = page.mask; bits != 0; bits &= bits - 1) {
                int slot = Long.numberOfTrailingZeros(bits);
                long key = ((page.key << 6) | slot) ^ Long.MIN_VALUE;
                Object value = page.valueAt(slot);
                result = function.invoke(result, MapEntry.create(key, value));
                if (RT.isReduced(result)) {
                    return result;
                }
            }
        }
        return result;
    }

    @Override
    public Object reduce(IFn function) {
        Iterator<Object> entries = iterator();
        if (!entries.hasNext()) {
            return function.invoke();
        }
        Object result = entries.next();
        while (entries.hasNext()) {
            result = function.invoke(result, entries.next());
            if (RT.isReduced(result)) {
                return ((IDeref) result).deref();
            }
        }
        return result;
    }

    @Override
    public Object reduce(IFn function, Object init) {
        if (root == null) {
            return init;
        }
        Object result = reduceEntries(root, function, init);
        return RT.isReduced(result) ? ((IDeref) result).deref() : result;
    }

    @Override
    public Object kvreduce(IFn function, Object init) {
        if (root == null) {
            return init;
        }
        // A long-returning reducer cannot return Reduced, so its accumulator
        // can stay primitive across every page and branch.
        if (function instanceof IFn.LLOL primitive && init instanceof Number) {
            return reduceLong(root, primitive, accumulator(init));
        }
        // Object-returning reducers retain early termination and nil semantics.
        Object result = function instanceof IFn.LLOO primitive && init instanceof Number
                ? reducePrimitive(root, primitive, init) : reduceKeyValues(root, function, init);
        return RT.isReduced(result) ? ((IDeref) result).deref() : result;
    }

    private static Object reduceKeyValues(Node node, IFn function, Object result) {
        if (node instanceof Branch branch) {
            for (long bits = branch.mask; bits != 0; bits &= bits - 1) {
                result = reduceKeyValues(branch.child(Long.numberOfTrailingZeros(bits)), function, result);
                if (RT.isReduced(result)) {
                    return result;
                }
            }
            return result;
        }
        return node == null ? result : reducePage((Page) node, function, result);
    }

    private static Object reducePage(Page page, IFn function, Object result) {
        long base = (page.key << 6) ^ Long.MIN_VALUE;
        Object[] values = page.values;
        if (values == null) {
            return function.invoke(result, base | Long.numberOfTrailingZeros(page.mask), page.single);
        }
        if (values.length == 64 && page.mask == -1L) {
            for (int index = 0; index < 64; ++index) {
                result = function.invoke(result, base | index, values[index]);
                if (RT.isReduced(result)) {
                    return result;
                }
            }
            return result;
        }
        int index = 0;
        for (long bits = page.mask; bits != 0; bits &= bits - 1) {
            int slot = Long.numberOfTrailingZeros(bits);
            result = function.invoke(result, base | slot, values[values.length == 64 ? slot : index++]);
            if (RT.isReduced(result)) {
                return result;
            }
        }
        return result;
    }

    private static Object reducePrimitive(Node node, IFn.LLOO function, Object result) {
        if (node instanceof Branch branch) {
            for (long bits = branch.mask; bits != 0; bits &= bits - 1) {
                result = reducePrimitive(branch.child(Long.numberOfTrailingZeros(bits)), function, result);
                if (RT.isReduced(result)) {
                    return result;
                }
            }
            return result;
        }
        if (node == null) {
            return result;
        }
        return reducePrimitivePage((Page) node, function, result);
    }

    private static long reduceLong(Node node, IFn.LLOL function, long result) {
        if (node instanceof Branch branch) {
            for (long bits = branch.mask; bits != 0; bits &= bits - 1) {
                result = reduceLong(branch.child(Long.numberOfTrailingZeros(bits)), function, result);
            }
            return result;
        }
        return reduceLongPage((Page) node, function, result);
    }

    private static long reduceLongPage(Page page, IFn.LLOL function, long result) {
        long base = (page.key << 6) ^ Long.MIN_VALUE;
        Object[] values = page.values;
        if (values == null) {
            return function.invokePrim(result, base | Long.numberOfTrailingZeros(page.mask), page.single);
        }
        if (values.length == 64 && page.mask == -1L) {
            for (int index = 0; index < 64; ++index) {
                result = function.invokePrim(result, base | index, values[index]);
            }
            return result;
        }
        int index = 0;
        for (long bits = page.mask; bits != 0; bits &= bits - 1) {
            int slot = Long.numberOfTrailingZeros(bits);
            result = function.invokePrim(result, base | slot, values[values.length == 64 ? slot : index++]);
        }
        return result;
    }

    private static long accumulator(Object result) {
        if (result instanceof Long number) {
            return number;
        }
        return RT.longCast(result);
    }

    private static Object reducePrimitivePage(Page page, IFn.LLOO function, Object result) {
        long base = (page.key << 6) ^ Long.MIN_VALUE;
        Object[] values = page.values;
        if (values == null) {
            return function.invokePrim(accumulator(result), base | Long.numberOfTrailingZeros(page.mask), page.single);
        }
        if (values.length == 64 && page.mask == -1L) {
            for (int index = 0; index < 64; ++index) {
                result = function.invokePrim(accumulator(result), base | index, values[index]);
                if (RT.isReduced(result)) {
                    return result;
                }
            }
            return result;
        }
        int index = 0;
        for (long bits = page.mask; bits != 0; bits &= bits - 1) {
            int slot = Long.numberOfTrailingZeros(bits);
            result = function.invokePrim(accumulator(result), base | slot, values[values.length == 64 ? slot : index++]);
            if (RT.isReduced(result)) {
                return result;
            }
        }
        return result;
    }

    private abstract static class Node implements Serializable {
        private static final long serialVersionUID = 1L;
        final long key;
        // Ownership is an identity token, not the editor or its thread. Deserialized
        // nodes have no owner and will be copied by the next transient edit.
        final transient Object owner;

        Node(long key, Object owner) {
            this.key = key;
            this.owner = owner;
        }
    }

    private static final class Page extends Node {
        private static final long serialVersionUID = 1L;
        long mask;
        Object single;
        Object[] values;

        Page(long key, Object owner, long mask, Object single, Object[] values) {
            super(key, owner);
            this.mask = mask;
            this.single = single;
            this.values = values;
        }

        Page editable(Object owner) {
            return this.owner == owner
                    ? this
                    : new Page(key, owner, mask, single, values == null ? null : values.clone());
        }

        Object valueAt(int slot) {
            return values == null ? single
                    : values[values.length == 64 ? slot : Long.bitCount(mask & ((1L << slot) - 1))];
        }

        void set(int slot, Object value) {
            long bit = 1L << slot;
            if (values == null) {
                if (mask == bit) {
                    single = value;
                } else {
                    values = Long.numberOfTrailingZeros(mask) < slot
                            ? new Object[] {single, value} : new Object[] {value, single};
                    single = null;
                    mask |= bit;
                }
                return;
            }
            if (values.length == 64) {
                values[slot] = value;
            } else {
                int index = Long.bitCount(mask & (bit - 1));
                if ((mask & bit) != 0) {
                    values[index] = value;
                } else {
                    int size = Long.bitCount(mask);
                    // Hysteresis avoids repeatedly packing/unpacking near a threshold.
                    if (size == 8) {
                        Object[] dense = new Object[64];
                        int i = 0;
                        for (long bits = mask; bits != 0; bits &= bits - 1) {
                            dense[Long.numberOfTrailingZeros(bits)] = values[i++];
                        }
                        dense[slot] = value;
                        values = dense;
                    } else {
                        if (values.length == size) {
                            values = Arrays.copyOf(values, size * 2);
                        }
                        System.arraycopy(values, index, values, index + 1, size - index);
                        values[index] = value;
                    }
                }
            }
            mask |= bit;
        }

        // Copy directly into the surviving representation. In particular, do not
        // clone a dense array just to discard it when this page becomes small.
        Page removed(int slot, Object owner) {
            long remaining = mask & ~(1L << slot);
            if (remaining == 0) {
                return null;
            }
            if (owner != null && this.owner == owner) {
                remove(slot);
                return this;
            }
            int size = Long.bitCount(remaining);
            if (size == 1) {
                return new Page(key, owner, remaining, valueAt(Long.numberOfTrailingZeros(remaining)), null);
            }
            Object[] copy;
            if (values.length == 64) {
                if (size == 4) {
                    copy = new Object[4];
                    int index = 0;
                    for (long bits = remaining; bits != 0; bits &= bits - 1) {
                        copy[index++] = values[Long.numberOfTrailingZeros(bits)];
                    }
                } else {
                    copy = values.clone();
                    copy[slot] = null;
                }
            } else {
                int index = Long.bitCount(mask & ((1L << slot) - 1));
                copy = new Object[size <= values.length / 4 ? values.length / 2 : values.length];
                System.arraycopy(values, 0, copy, 0, index);
                System.arraycopy(values, index + 1, copy, index, size - index);
            }
            return new Page(key, owner, remaining, null, copy);
        }

        void remove(int slot) {
            long bit = 1L << slot;
            int size = Long.bitCount(mask);
            if (size == 2) {
                single = valueAt(Long.numberOfTrailingZeros(mask & ~bit));
                values = null;
            } else if (values.length == 64) {
                values[slot] = null;
                if (size == 5) {
                    Object[] packed = new Object[4];
                    int i = 0;
                    for (long bits = mask & ~bit; bits != 0; bits &= bits - 1) {
                        packed[i++] = values[Long.numberOfTrailingZeros(bits)];
                    }
                    values = packed;
                }
            } else {
                int index = Long.bitCount(mask & (bit - 1));
                System.arraycopy(values, index + 1, values, index, size - index - 1);
                values[size - 1] = null;
                if (size - 1 <= values.length / 4) {
                    values = Arrays.copyOf(values, values.length / 2);
                }
            }
            mask &= ~bit;
        }
    }

    private static final class Branch extends Node {
        private static final long serialVersionUID = 1L;
        final int shift;
        long mask;
        Node[] children;
        // Dense branches copy only the edited half during a persistent update.
        Node[] upper;

        Branch(long key, Object owner, int shift, long mask, Node[] children) {
            this(key, owner, shift, mask, children, null);
        }

        Branch(long key, Object owner, int shift, long mask, Node[] children, Node[] upper) {
            super(key, owner);
            this.shift = shift;
            this.mask = mask;
            this.children = children;
            this.upper = upper;
        }

        boolean matches(long key) {
            return ((key ^ this.key) >>> (shift + 6)) == 0;
        }

        Branch editable(Object owner) {
            return this.owner == owner ? this : new Branch(key, owner, shift, mask,
                    children.clone(), upper == null ? null : upper.clone());
        }

        Branch copyPersistent(int slot) {
            return upper == null
                    ? new Branch(key, null, shift, mask, children.clone())
                    : new Branch(key, null, shift, mask,
                            slot < 32 ? children.clone() : children,
                            slot < 32 ? upper : upper.clone());
        }

        // The slot already exists. Copy only its final contents, without an
        // insertion path or a dense array that an eight-child pack discards.
        Branch afterRemoval(int slot, Node child, Object owner) {
            if (owner != null && this.owner == owner) {
                setChild(slot, child);
                return this;
            }
            long remaining = child == null ? mask & ~(1L << slot) : mask;
            if (upper != null) {
                if (child == null && Long.bitCount(remaining) == 8) {
                    Node[] packed = new Node[8];
                    int index = 0;
                    for (long bits = remaining; bits != 0; bits &= bits - 1) {
                        packed[index++] = child(Long.numberOfTrailingZeros(bits));
                    }
                    return new Branch(key, owner, shift, remaining, packed);
                }
                Node[] lower = owner != null || slot < 32 ? children.clone() : children;
                Node[] higher = owner != null || slot >= 32 ? upper.clone() : upper;
                (slot < 32 ? lower : higher)[slot & 31] = child;
                return new Branch(key, owner, shift, remaining, lower, higher);
            }
            int index = Long.bitCount(mask & ((1L << slot) - 1));
            Node[] copy;
            if (child == null) {
                copy = new Node[children.length];
                System.arraycopy(children, 0, copy, 0, index);
                System.arraycopy(children, index + 1, copy, index, Long.bitCount(remaining) - index);
            } else {
                copy = children.clone();
                copy[index] = child;
            }
            return new Branch(key, owner, shift, remaining, copy);
        }

        Node child(int slot) {
            return upper != null ? (slot < 32 ? children : upper)[slot & 31]
                    : (mask & (1L << slot)) == 0 ? null : children[Long.bitCount(mask & ((1L << slot) - 1))];
        }

        void replaceChild(int slot, Node child) {
            if (upper == null) {
                children[Long.bitCount(mask & ((1L << slot) - 1))] = child;
            } else {
                (slot < 32 ? children : upper)[slot & 31] = child;
            }
        }

        void setChild(int slot, Node child) {
            long bit = 1L << slot;
            if (child != null && (mask & bit) != 0) {
                replaceChild(slot, child);
                return;
            }
            int size = Long.bitCount(mask);
            if (upper != null) {
                (slot < 32 ? children : upper)[slot & 31] = child;
                mask = child == null ? mask & ~bit : mask | bit;
                if (child == null && size == 9) {
                    Node[] packed = new Node[8];
                    int i = 0;
                    for (long bits = mask; bits != 0; bits &= bits - 1) {
                        packed[i++] = child(Long.numberOfTrailingZeros(bits));
                    }
                    children = packed;
                    upper = null;
                }
                return;
            }
            int index = Long.bitCount(mask & (bit - 1));
            if ((mask & bit) != 0) {
                if (child == null) {
                    System.arraycopy(children, index + 1, children, index, size - index - 1);
                    children[size - 1] = null;
                    mask &= ~bit;
                } else {
                    children[index] = child;
                }
                return;
            }
            if (size == 16) {
                Node[] lower = new Node[32];
                Node[] higher = new Node[32];
                int i = 0;
                for (long bits = mask; bits != 0; bits &= bits - 1) {
                    int position = Long.numberOfTrailingZeros(bits);
                    (position < 32 ? lower : higher)[position & 31] = children[i++];
                }
                (slot < 32 ? lower : higher)[slot & 31] = child;
                children = lower;
                upper = higher;
            } else {
                if (children.length == size) {
                    children = Arrays.copyOf(children, size * 2);
                }
                System.arraycopy(children, index, children, index + 1, size - index);
                children[index] = child;
            }
            mask |= bit;
        }
    }

    /** Transient Clojure map with primitive assoc, without, get and containsKey methods. */
    public static final class Transient extends AFn implements ITransientMap, ITransientAssociative2 {
        private final Object owner = new Object();
        private LongMap source;
        private Node root;
        private int count;
        private boolean modified;
        private boolean updating;
        // This owned page remains reachable until its final entry is removed.
        private Page recent;

        private Transient(LongMap source) {
            this.source = source;
            root = source.root;
            count = source.count;
        }

        private void ensureEditable() {
            if (source == null) {
                throw new IllegalAccessError("Transient used after persistent!");
            }
        }

        private void ensureWritable() {
            ensureEditable();
            if (updating) {
                throw new IllegalStateException("Cannot mutate an editor from its update callback");
            }
        }

        public Transient update(long key, IFn function) {
            return update(key, function, false);
        }

        public Transient updateIfPresent(long key, IFn function) {
            return update(key, function, true);
        }

        private Transient update(long key, IFn function, boolean presentOnly) {
            ensureWritable();
            java.util.Objects.requireNonNull(function);
            updating = true;
            try {
                long pageKey = (key ^ Long.MIN_VALUE) >>> 6;
                int slot = (int) key & 63;
                if (recent != null && recent.key == pageKey) {
                    updateNode(recent, pageKey, slot, function, presentOnly);
                } else {
                    root = updateNode(root, pageKey, slot, function, presentOnly);
                }
                return this;
            } finally {
                updating = false;
            }
        }

        private Node updateNode(Node node, long key, int slot, IFn function, boolean presentOnly) {
            if (node instanceof Branch branch && branch.matches(key)) {
                int index = (int) (key >>> branch.shift) & 63;
                Node old = branch.child(index);
                Node child = updateNode(old, key, slot, function, presentOnly);
                if (child == old) {
                    return branch;
                }
                Branch result = branch.editable(owner);
                result.setChild(index, child);
                return result;
            }
            Page page = node instanceof Page candidate && candidate.key == key ? candidate : null;
            boolean present = page != null && (page.mask & (1L << slot)) != 0;
            if (!present && presentOnly) {
                return node;
            }
            Object value = function.invoke(page != null && present ? page.valueAt(slot) : null);
            return insert(node, key, slot, value);
        }

        public Transient assoc(long key, Object value) {
            ensureWritable();
            long pageKey = (key ^ Long.MIN_VALUE) >>> 6;
            int slot = (int) key & 63;
            if (recent != null && recent.key == pageKey) {
                boolean present = (recent.mask & (1L << slot)) != 0;
                if (!present || recent.valueAt(slot) != value) {
                    if (!present) {
                        ++count;
                    }
                    modified = true;
                    recent.set(slot, value);
                }
            } else {
                root = insert(root, pageKey, slot, value);
            }
            return this;
        }

        private Node insert(Node node, long key, int slot, Object value) {
            if (node == null) {
                ++count;
                modified = true;
                return recent = new Page(key, owner, 1L << slot, value, null);
            }
            if (node instanceof Branch branch) {
                if (!branch.matches(key)) {
                    return join(branch, insert(null, key, slot, value));
                }
                int index = (int) (key >>> branch.shift) & 63;
                Node old = branch.child(index);
                Node child = insert(old, key, slot, value);
                if (child == old) {
                    return branch;
                }
                Branch result = branch.editable(owner);
                result.setChild(index, child);
                return result;
            }
            Page page = (Page) node;
            if (page.key != key) {
                return join(page, insert(null, key, slot, value));
            }
            boolean present = (page.mask & (1L << slot)) != 0;
            if (present && page.valueAt(slot) == value) {
                recent = page.owner == owner ? page : null;
                return page;
            }
            if (!present) {
                ++count;
            }
            modified = true;
            Page result = page.editable(owner);
            result.set(slot, value);
            recent = result;
            return result;
        }

        private Branch join(Node left, Node right) {
            int shift = ((63 - Long.numberOfLeadingZeros(left.key ^ right.key)) / 6) * 6;
            int leftSlot = (int) (left.key >>> shift) & 63;
            int rightSlot = (int) (right.key >>> shift) & 63;
            return new Branch(left.key, owner, shift, (1L << leftSlot) | (1L << rightSlot),
                    leftSlot < rightSlot ? new Node[] {left, right} : new Node[] {right, left});
        }

        public Transient without(long key) {
            ensureWritable();
            long pageKey = (key ^ Long.MIN_VALUE) >>> 6;
            int slot = (int) key & 63;
            if (recent != null && recent.key == pageKey) {
                long bit = 1L << slot;
                if ((recent.mask & bit) == 0) {
                    return this;
                }
                if (recent.mask != bit) {
                    recent.remove(slot);
                    --count;
                    modified = true;
                    return this;
                }
                recent = null;
            }
            root = remove(root, pageKey, slot);
            return this;
        }

        private Node remove(Node node, long key, int slot) {
            if (node instanceof Branch branch) {
                if (!branch.matches(key)) {
                    return branch;
                }
                int index = (int) (key >>> branch.shift) & 63;
                Node old = branch.child(index);
                Node child = remove(old, key, slot);
                if (child == old) {
                    return branch;
                }
                if (child == null && Long.bitCount(branch.mask) == 2) {
                    return branch.child(Long.numberOfTrailingZeros(branch.mask & ~(1L << index)));
                }
                return branch.afterRemoval(index, child, owner);
            }
            if (node == null || node.key != key || (((Page) node).mask & (1L << slot)) == 0) {
                return node;
            }
            --count;
            modified = true;
            Page page = (Page) node;
            Page result = page.removed(slot, owner);
            if (result != null) {
                recent = result;
            }
            return result;
        }

        public Object get(long key) {
            return get(key, null);
        }

        public Object get(long key, Object notFound) {
            ensureEditable();
            long pageKey = (key ^ Long.MIN_VALUE) >>> 6;
            if (recent != null && recent.key == pageKey) {
                int slot = (int) key & 63;
                return (recent.mask & (1L << slot)) == 0 ? notFound : recent.valueAt(slot);
            }
            return find(root, key, notFound);
        }

        public boolean containsKey(long key) {
            return get(key, NOT_FOUND) != NOT_FOUND;
        }

        @Override
        public Transient assoc(Object key, Object value) {
            ensureEditable();
            return assoc(checkedKey(key), value);
        }

        @Override
        public Transient without(Object key) {
            ensureEditable();
            return isKey(key) ? without(((Number) key).longValue()) : this;
        }

        @Override
        public Object valAt(Object key) {
            return valAt(key, null);
        }

        @Override
        public Object valAt(Object key, Object notFound) {
            ensureEditable();
            return isKey(key) ? get(((Number) key).longValue(), notFound) : notFound;
        }

        @Override
        public boolean containsKey(Object key) {
            return valAt(key, NOT_FOUND) != NOT_FOUND;
        }

        @Override
        public IMapEntry entryAt(Object key) {
            Object value = valAt(key, NOT_FOUND);
            return value == NOT_FOUND ? null : MapEntry.create(((Number) key).longValue(), value);
        }

        @Override
        public Object invoke(Object key) {
            return valAt(key);
        }

        @Override
        public Object invoke(Object key, Object notFound) {
            return valAt(key, notFound);
        }

        @Override
        public int count() {
            ensureEditable();
            return count;
        }

        @Override
        public Transient conj(Object entry) {
            ensureEditable();
            if (entry instanceof Map.Entry<?, ?> pair) {
                return assoc(pair.getKey(), pair.getValue());
            }
            if (entry instanceof IPersistentVector pair) {
                if (pair.count() != 2) {
                    throw new IllegalArgumentException("Vector arg to map conj must be a pair");
                }
                return assoc(pair.nth(0), pair.nth(1));
            }
            for (ISeq entries = RT.seq(entry); entries != null; entries = entries.next()) {
                Map.Entry<?, ?> pair = (Map.Entry<?, ?>) entries.first();
                assoc(pair.getKey(), pair.getValue());
            }
            return this;
        }

        @Override
        public LongMap persistent() {
            ensureWritable();
            LongMap result = !modified ? source
                    : root == null ? source.empty() : new LongMap(root, count, source.meta);
            source = null;
            root = null;
            recent = null;
            return result;
        }
    }

    private static final class Cursor implements Iterator<Object> {
        // Page keys have 58 bits, so at most ten six-bit branch levels exist.
        private final Branch[] branches = new Branch[10];
        private final long[] remaining = new long[10];
        private final int mode;
        private final boolean reverse;
        private int depth;
        private Page page;
        private long slots;

        Cursor(Node root, int mode, boolean reverse) {
            this.mode = mode;
            this.reverse = reverse;
            if (root != null) {
                descend(root);
            }
        }

        private int firstSlot(long mask) {
            return reverse ? 63 - Long.numberOfLeadingZeros(mask) : Long.numberOfTrailingZeros(mask);
        }

        private void descend(Node node) {
            while (node instanceof Branch branch) {
                int slot = firstSlot(branch.mask);
                branches[depth] = branch;
                remaining[depth++] = branch.mask & ~(1L << slot);
                node = branch.child(slot);
            }
            page = (Page) node;
            slots = page.mask;
        }

        private void advance() {
            while (depth > 0) {
                int index = depth - 1;
                long mask = remaining[index];
                if (mask == 0) {
                    branches[--depth] = null;
                } else {
                    int slot = firstSlot(mask);
                    remaining[index] = mask & ~(1L << slot);
                    descend(branches[index].child(slot));
                    return;
                }
            }
            page = null;
        }

        @Override
        public boolean hasNext() {
            return page != null;
        }

        @Override
        public Object next() {
            if (page == null) {
                throw new NoSuchElementException();
            }
            int slot = reverse ? 63 - Long.numberOfLeadingZeros(slots) : Long.numberOfTrailingZeros(slots);
            long key = ((page.key << 6) | slot) ^ Long.MIN_VALUE;
            Object value = page.valueAt(slot);
            Object result = mode == 0 ? MapEntry.create(key, value) : mode == 1 ? key : value;
            slots &= ~(1L << slot);
            if (slots == 0) {
                advance();
            }
            return result;
        }
    }
}
