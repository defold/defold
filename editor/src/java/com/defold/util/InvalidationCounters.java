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
import clojure.lang.Counted;
import clojure.lang.IFn;
import clojure.lang.IReduceInit;
import clojure.lang.Keyword;
import clojure.lang.RT;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.function.Function;
import java.util.function.ToLongFunction;

/**
 * Self-contained persistent stamp counters. Compressed lifetime regions with compact mixed pages.
 */
public final class InvalidationCounters {

    public static final InvalidationCounters EMPTY =
            new InvalidationCounters(0, Lifetimes.EMPTY, Outputs.EMPTY);
    private final long generation;
    private final Lifetimes lifetimes;
    private final Outputs outputs;

    private InvalidationCounters(long generation, Lifetimes lifetimes, Outputs outputs) {
        this.generation = generation;
        this.lifetimes = lifetimes;
        this.outputs = outputs;
    }

    private static final IFn REDUCE = RT.var("clojure.core", "reduce");

    /** Return a known count, or -1, without reducing the input. */
    private static int knownCount(Object values) {
        if (values == null) {
            return 0;
        }
        if (values instanceof Counted) {
            return ((Counted) values).count();
        }
        if (values instanceof Collection) {
            return ((Collection<?>) values).isEmpty() ? 0 : -1;
        }
        if (values.getClass().isArray()) {
            return Array.getLength(values);
        }
        return -1;
    }

    private static void reduce(IFn operation, Object values) {
        if (values instanceof IReduceInit) {
            ((IReduceInit) values).reduce(operation, null);
        } else {
            REDUCE.invoke(operation, null, values);
        }
    }

    private static final class AddReducer extends AFn {
        final Editor editor;
        final long birth;

        AddReducer(Editor editor, long birth) {
            this.editor = editor;
            this.birth = birth;
        }

        @Override
        public Object invoke(Object accumulator, Object id) {
            editor.add(((Number) id).longValue(), birth);
            return accumulator;
        }
    }

    private static final class RemoveReducer extends AFn {
        final Editor editor;

        RemoveReducer(Editor editor) {
            this.editor = editor;
        }

        @Override
        public Object invoke(Object accumulator, Object id) {
            editor.remove(((Number) id).longValue());
            return accumulator;
        }
    }

    private static final class BumpReducer extends AFn {
        final Editor editor;
        final ToLongFunction<Object> nodeId;
        final Function<Object, Keyword> label;

        BumpReducer(Editor editor, ToLongFunction<Object> nodeId, Function<Object, Keyword> label) {
            this.editor = editor;
            this.nodeId = nodeId;
            this.label = label;
        }

        @Override
        public Object invoke(Object accumulator, Object endpoint) {
            editor.bump(nodeId.applyAsLong(endpoint), label.apply(endpoint));
            return accumulator;
        }
    }

    public InvalidationCounters addNodes(Object ids, long birth) {
        if (knownCount(ids) == 0) {
            return this;
        }
        return edit().addNodes(ids, birth).commit();
    }

    public InvalidationCounters removeNodes(Object ids) {
        if (knownCount(ids) == 0) {
            return this;
        }
        return edit().removeNodes(ids).commit();
    }

    /** Accessors read endpoint fields without creating intermediate endpoint pairs. */
    public InvalidationCounters bump(
            Object endpoints, ToLongFunction<Object> nodeId, Function<Object, Keyword> label) {
        if (knownCount(endpoints) == 0) {
            return this;
        }
        return edit().bump(endpoints, nodeId, label).commit();
    }

    public Editor edit() {
        return new Editor(this);
    }

    public InvalidationCounters fullInvalidate() {
        return new InvalidationCounters(generation + 1, lifetimes, Outputs.EMPTY);
    }

    public boolean fullInvalidatedSince(InvalidationCounters before) {
        return generation != before.generation;
    }

    public boolean changedSince(InvalidationCounters before, long id, Keyword label) {
        return !lifetimes.sameLifetime(before.lifetimes, id)
                || outputs.changedSince(before.outputs, id, label);
    }

    public Object counter(long id, Keyword label) {
        return outputs.counter(id, label);
    }

    public boolean contains(long id) {
        return lifetimes.contains(id);
    }

    /** Call only for a live ID. Zero is a valid lifetime, not a missing sentinel. */
    public long birth(long id) {
        return lifetimes.birth(id);
    }

    public int lifetimeCount() {
        return lifetimes.size;
    }

    public int outputNodeCount() {
        return outputs.nodeCount;
    }

    public int outputCount() {
        return outputs.outputCount;
    }

    /**
     * Mutable edit session. Confine to one thread and commit once. Published snapshots remain
     * immutable; no operation is allowed after commit.
     */
    public static final class Editor {
        private final InvalidationCounters source;
        private long generation;
        private Outputs initialOutputs;
        private LifetimeEditor lifetimes;
        private Outputs.Editor outputs;
        private boolean committed;

        private Editor(InvalidationCounters source) {
            this.source = source;
            this.generation = source.generation;
            this.initialOutputs = source.outputs;
        }

        private void ensureEditable() {
            if (committed) {
                throw new IllegalStateException("Editor already committed");
            }
        }

        public Editor addNodes(Object ids, long birth) {
            ensureEditable();
            int count = knownCount(ids);
            if (count == 0) {
                return this;
            }
            if (count > 0) {
                lifetimeEditor().prepareBatch(birth, count);
            }
            reduce(new AddReducer(this, birth), ids);
            return this;
        }

        public Editor removeNodes(Object ids) {
            ensureEditable();
            if (knownCount(ids) == 0) {
                return this;
            }
            reduce(new RemoveReducer(this), ids);
            return this;
        }

        /** Calls share an output stamp until full invalidation clears it. */
        public Editor bump(
                Object endpoints, ToLongFunction<Object> nodeId, Function<Object, Keyword> label) {
            ensureEditable();
            if (knownCount(endpoints) == 0) {
                return this;
            }
            reduce(new BumpReducer(this, nodeId, label), endpoints);
            return this;
        }

        public Editor fullInvalidate() {
            ensureEditable();
            ++generation;
            initialOutputs = Outputs.EMPTY;
            outputs = null;
            return this;
        }

        private LifetimeEditor lifetimeEditor() {
            if (lifetimes == null) {
                lifetimes = new LifetimeEditor(source.lifetimes);
            }
            return lifetimes;
        }

        private Outputs.Editor outputEditor() {
            if (outputs == null) {
                outputs = initialOutputs.edit(this);
            }
            return outputs;
        }

        private boolean contains(long id) {
            return lifetimes == null ? source.contains(id) : lifetimes.contains(id);
        }

        private void add(long id, long birth) {
            lifetimeEditor().add(id, birth);
        }

        private void remove(long id) {
            lifetimeEditor().remove(id);
            outputEditor().remove(id);
        }

        private void bump(long id, Keyword label) {
            if (label == null) {
                throw new IllegalArgumentException("Output label must be a keyword");
            }
            outputEditor().bump(id, label);
        }

        public InvalidationCounters commit() {
            ensureEditable();
            Lifetimes live = lifetimes == null ? source.lifetimes : lifetimes.finish();
            Outputs versions = outputs == null ? initialOutputs : outputs.finish();
            committed = true;
            return generation == source.generation
                            && live == source.lifetimes
                            && versions == source.outputs
                    ? source
                    : new InvalidationCounters(generation, live, versions);
        }
    }

    private static final class Lifetimes {
        static final Lifetimes EMPTY = new Lifetimes(null, 0);
        final LifetimeTree.Node root;
        final int size;

        Lifetimes(LifetimeTree.Node root, int size) {
            this.root = root;
            this.size = size;
        }

        boolean contains(long id) {
            return (LifetimeTree.mask(LifetimeTree.find(root, id >>> 6)) & (1L << id)) != 0;
        }

        long birth(long id) {
            return LifetimeTree.birth(LifetimeTree.find(root, id >>> 6), (int) id & 63);
        }

        boolean sameLifetime(Lifetimes other, long id) {
            if (this == other) {
                return contains(id);
            }
            LifetimeTree.Node current = LifetimeTree.find(root, id >>> 6);
            long selected = 1L << id;
            if ((LifetimeTree.mask(current) & selected) == 0) {
                return false;
            }
            LifetimeTree.Node previous = LifetimeTree.find(other.root, id >>> 6);
            if ((LifetimeTree.mask(previous) & selected) == 0) {
                return false;
            }
            int slot = (int) id & 63;
            return LifetimeTree.birth(current, slot) == LifetimeTree.birth(previous, slot);
        }
    }

    private static final class LifetimeEditor {
        final Lifetimes source;
        final Object owner = new Object();
        int size;
        PageBatch batch;
        long batchBirth;

        final LifetimeTree.TreeEdit tree;

        LifetimeEditor(Lifetimes source) {
            this.source = source;
            size = source.size;
            tree = new LifetimeTree.TreeEdit(source.root, owner);
        }

        boolean contains(long id) {
            flush();
            return (LifetimeTree.mask(LifetimeTree.find(tree.root, id >>> 6)) & (1L << id)) != 0;
        }

        void addPage(long key, long selected, long birth) {
            size += tree.assign(key, selected, birth);
        }

        void remove(long id) {
            flush();
            size += tree.remove(id >>> 6, 1L << id);
        }

        Lifetimes finish() {
            flush();
            batch = null;
            return tree.root == source.root ? source : new Lifetimes(tree.root, size);
        }

        void prepareBatch(long birth, int inputCount) {
            flush();
            batch = inputCount < 4096 ? null : new PageBatch(inputCount);
            batchBirth = birth;
        }

        void add(long id, long birth) {
            if (batch == null) {
                addPage(id >>> 6, 1L << id, birth);
                return;
            }
            if (batchBirth != birth) {
                flush();
                batchBirth = birth;
            }
            if (!batch.add(id >>> 6, 1L << id)) {
                flush();
                batch.add(id >>> 6, 1L << id);
            }
        }

        void flush() {
            if (batch == null || batch.size == 0) {
                return;
            }
            for (int i = 0; i < batch.size; i++) {
                int slot = batch.occupied[i];
                addPage(batch.keys[slot], batch.masks[slot], batchBirth);
                batch.masks[slot] = 0;
            }
            batch.size = 0;
        }
    }

    /** Confined primitive scratch buffer, capped at 1.125 MiB and reused after flushing. */
    private static final class PageBatch {
        final long[] keys;
        final long[] masks;
        final int[] occupied;
        int size;

        PageBatch(int inputCount) {
            int expectedPages = Math.max(64, Math.min(32768, inputCount >>> 6));
            int capacity = 128;
            while (capacity < expectedPages * 2) {
                capacity <<= 1;
            }
            keys = new long[capacity];
            masks = new long[capacity];
            occupied = new int[capacity / 2];
        }

        boolean add(long key, long selected) {
            long hash = key;
            hash = (hash ^ (hash >>> 30)) * 0xbf58476d1ce4e5b9L;
            hash = (hash ^ (hash >>> 27)) * 0x94d049bb133111ebL;
            int slot = (int) (hash ^ (hash >>> 31)) & (masks.length - 1);
            while (masks[slot] != 0 && keys[slot] != key) {
                slot = (slot + 1) & (masks.length - 1);
            }
            if (masks[slot] == 0) {
                if (size == masks.length / 2) {
                    return false;
                }
                keys[slot] = key;
                occupied[size++] = slot;
            }
            masks[slot] |= selected;
            return true;
        }
    }

    private static final class LifetimeTree {

        abstract static class Node {
            final long key;
            final Object owner;

            Node(long key, Object owner) {
                this.key = key;
                this.owner = owner;
            }
        }

        static final class Branch extends Node {
            final int shift;
            final Node[] children;
            int count;

            Branch(long key, int shift, Node[] children, int count, Object owner) {
                super(key, owner);
                this.shift = shift;
                this.children = children;
                this.count = count;
            }

            Branch editable(Object owner) {
                return this.owner == owner
                        ? this
                        : new Branch(key, shift, children.clone(), count, owner);
            }
        }

        /** Every page in this range is full and has the same lifetime. */
        static final class Uniform extends Node {
            final int bits;
            final long birth;

            Uniform(long key, int bits, long birth) {
                super(key, null);
                this.bits = bits;
                this.birth = birth;
            }
        }

        abstract static class Page extends Node {
            long mask;

            Page(long key, long mask, Object owner) {
                super(key, owner);
                this.mask = mask;
            }

            abstract Page copy(Object owner);

            abstract long birth(int slot);

            abstract void assign(long mask, long birth);

            abstract void remove(long mask);
        }

        /**
         * Uniform lifetime, one inline exception, up to eight packed exceptions, then a dense
         * primitive array. No array is needed for a single exception.
         */
        static final class ValuesPage extends Page {
            long uniform;
            long exceptions;
            long single;
            long[] values;

            ValuesPage(long key, long mask, long birth, Object owner) {
                super(key, mask, owner);
                uniform = birth;
            }

            Page copy(Object owner) {
                ValuesPage copy = new ValuesPage(key, mask, uniform, owner);
                copy.exceptions = exceptions;
                copy.single = single;
                copy.values = values == null ? null : values.clone();
                return copy;
            }

            long birth(int slot) {
                long bit = 1L << slot;
                if ((exceptions & bit) == 0) {
                    return uniform;
                }
                if (values == null) {
                    return single;
                }
                return values.length == 64
                        ? values[slot]
                        : values[Long.bitCount(exceptions & (bit - 1))];
            }

            void assign(long selected, long birth) {
                if (mask == 0 || (mask & ~selected) == 0) {
                    uniform = birth;
                    exceptions = 0;
                    values = null;
                    mask |= selected;
                    return;
                }
                if (birth == uniform) {
                    removeExceptions(selected);
                } else {
                    for (long bits = selected; bits != 0; bits &= bits - 1) {
                        assignException(Long.numberOfTrailingZeros(bits), birth);
                    }
                }
                mask |= selected;
                compress();
            }

            private void assignException(int slot, long birth) {
                long bit = 1L << slot;
                int count = Long.bitCount(exceptions);
                int index = Long.bitCount(exceptions & (bit - 1));
                if ((exceptions & bit) != 0) {
                    if (values == null) {
                        single = birth;
                    } else {
                        values[values.length == 64 ? slot : index] = birth;
                    }
                    return;
                }
                if (count == 0) {
                    single = birth;
                } else if (values != null && values.length == 64) {
                    values[slot] = birth;
                } else if (count < 8) {
                    long[] added = new long[count + 1];
                    if (count == 1) {
                        added[index == 0 ? 1 : 0] = single;
                    } else {
                        System.arraycopy(values, 0, added, 0, index);
                        System.arraycopy(values, index, added, index + 1, count - index);
                    }
                    added[index] = birth;
                    values = added;
                } else {
                    long[] dense = new long[64];
                    Arrays.fill(dense, uniform);
                    for (long bits = exceptions; bits != 0; bits &= bits - 1) {
                        int previous = Long.numberOfTrailingZeros(bits);
                        dense[previous] = birth(previous);
                    }
                    dense[slot] = birth;
                    values = dense;
                }
                exceptions |= bit;
            }

            void remove(long selected) {
                removeExceptions(selected);
                mask &= ~selected;
                compress();
            }

            private void removeExceptions(long selected) {
                long remaining = exceptions & ~selected;
                if (remaining == exceptions) {
                    return;
                }
                int count = Long.bitCount(remaining);
                if (count == 0) {
                    values = null;
                } else if (count == 1) {
                    single = birth(Long.numberOfTrailingZeros(remaining));
                    values = null;
                } else if (count <= 8) {
                    long[] packed = new long[count];
                    int index = 0;
                    for (long bits = remaining; bits != 0; bits &= bits - 1) {
                        packed[index++] = birth(Long.numberOfTrailingZeros(bits));
                    }
                    values = packed;
                }
                exceptions = remaining;
            }

            private void compress() {
                if (mask == 0) {
                    exceptions = 0;
                    values = null;
                    return;
                }
                if (exceptions != mask) {
                    return;
                }
                long candidate = birth(Long.numberOfTrailingZeros(mask));
                for (long bits = mask; bits != 0; bits &= bits - 1) {
                    if (birth(Long.numberOfTrailingZeros(bits)) != candidate) {
                        return;
                    }
                }
                uniform = candidate;
                exceptions = 0;
                values = null;
            }
        }

        private static int shift(long a, long b) {
            return ((63 - Long.numberOfLeadingZeros(a ^ b)) / 5) * 5;
        }

        private static boolean matches(Node node, long key) {
            int bits =
                    node instanceof Branch
                            ? Math.min(58, ((Branch) node).shift + 5)
                            : node instanceof Uniform ? ((Uniform) node).bits : 0;
            return ((key ^ node.key) >>> bits) == 0;
        }

        static Node find(Node node, long key) {
            while (node instanceof Branch) {
                if (!matches(node, key)) {
                    return null;
                }
                Branch branch = (Branch) node;
                node = branch.children[(int) (key >>> branch.shift) & 31];
            }
            return node != null && matches(node, key) ? node : null;
        }

        static long mask(Node node) {
            return node == null ? 0 : node instanceof Uniform ? -1L : ((Page) node).mask;
        }

        static long birth(Node node, int slot) {
            return node instanceof Uniform ? ((Uniform) node).birth : ((Page) node).birth(slot);
        }

        static final class TreeEdit {
            Node root;
            final Object owner;
            int delta;

            TreeEdit(Node root, Object owner) {
                this.root = root;
                this.owner = owner;
            }

            int assign(long key, long selected, long birth) {
                return update(key, selected, birth, false);
            }

            int remove(long key, long selected) {
                return update(key, selected, 0, true);
            }

            private int update(long key, long selected, long birth, boolean remove) {
                delta = 0;
                root = update(root, key, selected, birth, remove);
                return delta;
            }

            boolean sameValue(Node node, long selected, long birth) {
                for (long bits = selected; bits != 0; bits &= bits - 1) {
                    if (birth(node, Long.numberOfTrailingZeros(bits)) != birth) {
                        return false;
                    }
                }
                return true;
            }

            Node createPage(long key, long selected, long birth) {
                return selected == -1L
                        ? new Uniform(key, 0, birth)
                        : new ValuesPage(key, selected, birth, owner);
            }

            Page editableUniform(Uniform uniform) {
                return new ValuesPage(uniform.key, -1L, uniform.birth, owner);
            }

            Node normalize(Page page) {
                ValuesPage values = (ValuesPage) page;
                return values.mask == -1L && values.exceptions == 0
                        ? new Uniform(values.key, 0, values.uniform)
                        : values;
            }

            Node normalize(Branch branch) {
                return compress(branch);
            }

            private Branch join(Node left, Node right) {
                int shift = shift(left.key, right.key);
                Node[] children = new Node[32];
                children[(int) (left.key >>> shift) & 31] = left;
                children[(int) (right.key >>> shift) & 31] = right;
                return new Branch(left.key, shift, children, 2, owner);
            }

            private Node update(Node node, long key, long selected, long birth, boolean remove) {
                if (node == null || !matches(node, key)) {
                    if (remove) {
                        return node;
                    }
                    delta = Long.bitCount(selected);
                    Node added = createPage(key, selected, birth);
                    return node == null ? added : join(node, added);
                }
                if (node instanceof Uniform && ((Uniform) node).bits > 0) {
                    Uniform uniform = (Uniform) node;
                    if (!remove && uniform.birth == birth) {
                        return node;
                    }
                    int shift = uniform.bits == 58 ? 55 : uniform.bits - 5;
                    int count = uniform.bits == 58 ? 8 : 32;
                    Node[] children = new Node[32];
                    for (int i = 0; i < count; i++) {
                        children[i] =
                                new Uniform(
                                        uniform.key + ((long) i << shift), shift, uniform.birth);
                    }
                    node = new Branch(uniform.key, shift, children, count, owner);
                }
                if (node instanceof Branch) {
                    Branch branch = (Branch) node;
                    int index = (int) (key >>> branch.shift) & 31;
                    Node old = branch.children[index];
                    Node updated = update(old, key, selected, birth, remove);
                    if (old == updated) {
                        return node;
                    }
                    Branch result = branch.editable(owner);
                    result.children[index] = updated;
                    if (old == null) {
                        result.count++;
                    }
                    if (updated == null) {
                        result.count--;
                    }
                    if (result.count == 0) {
                        return null;
                    }
                    if (result.count == 1) {
                        for (Node child : result.children) {
                            if (child != null) {
                                return child;
                            }
                        }
                    }
                    return normalize(result);
                }
                long oldMask = mask(node);
                delta =
                        remove
                                ? -Long.bitCount(oldMask & selected)
                                : Long.bitCount(selected & ~oldMask);
                if (remove && (oldMask & selected) == 0) {
                    return node;
                }
                if (!remove && (oldMask & selected) == selected) {
                    if (sameValue(node, selected, birth)) {
                        return node;
                    }
                }
                Page page =
                        node instanceof Uniform
                                ? editableUniform((Uniform) node)
                                : node.owner == owner ? (Page) node : ((Page) node).copy(owner);
                if (remove) {
                    page.remove(selected);
                } else {
                    page.assign(selected, birth);
                }
                if (page.mask == 0) {
                    return null;
                }
                return normalize(page);
            }

            private Node compress(Branch branch) {
                int count = branch.shift == 55 ? 8 : 32;
                if (branch.count != count) {
                    return branch;
                }
                Uniform first = null;
                for (int i = 0; i < count; i++) {
                    Node child = branch.children[i];
                    if (!(child instanceof Uniform)) {
                        return branch;
                    }
                    Uniform uniform = (Uniform) child;
                    if (uniform.bits != branch.shift
                            || (first != null && uniform.birth != first.birth)) {
                        return branch;
                    }
                    first = uniform;
                }
                int bits = Math.min(58, branch.shift + 5);
                return new Uniform(branch.key & (-1L << bits), bits, first.birth);
            }
        }
    }

    private static final class Outputs {
        static final Outputs EMPTY = new Outputs(null, 0, 0);

        private final Node root;
        public final int nodeCount;
        public final int outputCount;

        private Outputs(Node root, int nodeCount, int outputCount) {
            this.root = root;
            this.nodeCount = nodeCount;
            this.outputCount = outputCount;
        }

        Editor edit(InvalidationCounters.Editor live) {
            return new Editor(this, live);
        }

        public Outputs clear() {
            return EMPTY;
        }

        public Object counter(long nodeId, Keyword label) {
            Leaf leaf = find(root, nodeId);
            if (leaf == null) {
                return null;
            }
            return stamp(leaf, label);
        }

        /** Global resets and node lifetimes are checked by the enclosing snapshot. */
        public boolean changedSince(Outputs old, long nodeId, Keyword label) {
            if (root == old.root) {
                return false;
            }
            Leaf before = find(old.root, nodeId);
            Leaf after = find(root, nodeId);
            if (before == after) {
                return false;
            }
            return stamp(before, label) != stamp(after, label);
        }

        private abstract static class Node {
            final long key;
            final Object owner;

            Node(long key, Object owner) {
                this.key = key;
                this.owner = owner;
            }
        }

        // Sparse branches use packed children with a bitmap. Owned arrays grow
        // geometrically; dense branches keep direct indexing for high fanout.
        private static final class Branch extends Node {
            final int shift;
            Node[] children;
            int bitmap;

            Branch(long key, int shift, Node[] children, int bitmap, Object owner) {
                super(key, owner);
                this.shift = shift;
                this.children = children;
                this.bitmap = bitmap;
            }

            boolean matches(long key) {
                return shift == 60 || ((key ^ this.key) >>> (shift + 5)) == 0;
            }

            Branch editable(Object owner) {
                return this.owner == owner
                        ? this
                        : new Branch(key, shift, children.clone(), bitmap, owner);
            }

            Node child(int slot) {
                int bit = 1 << slot;
                if (children.length == 32) {
                    return children[slot];
                }
                return (bitmap & bit) == 0 ? null : children[Integer.bitCount(bitmap & (bit - 1))];
            }

            void setChild(int slot, Node child) {
                int bit = 1 << slot;
                int size = Integer.bitCount(bitmap);
                if (children.length == 32) {
                    children[slot] = child;
                    if (child == null) {
                        bitmap &= ~bit;
                    } else {
                        bitmap |= bit;
                    }
                    if (child == null && size == 5) {
                        Node[] packed = new Node[4];
                        int index = 0;
                        for (Node remaining : children) {
                            if (remaining != null) {
                                packed[index++] = remaining;
                            }
                        }
                        children = packed;
                    }
                    return;
                }
                int index = Integer.bitCount(bitmap & (bit - 1));
                if ((bitmap & bit) != 0) {
                    if (child == null) {
                        System.arraycopy(children, index + 1, children, index, size - index - 1);
                        children[size - 1] = null;
                        bitmap &= ~bit;
                    } else {
                        children[index] = child;
                    }
                } else {
                    // Keep at most eight packed children; use direct indexing above that.
                    if (size == 8) {
                        Node[] dense = new Node[32];
                        int packedIndex = 0;
                        for (int bits = bitmap; bits != 0; bits &= bits - 1) {
                            dense[Integer.numberOfTrailingZeros(bits)] = children[packedIndex++];
                        }
                        dense[slot] = child;
                        children = dense;
                    } else {
                        if (size == children.length) {
                            children = Arrays.copyOf(children, Math.max(2, size * 2));
                        }
                        System.arraycopy(children, index, children, index + 1, size - index);
                        children[index] = child;
                    }
                    bitmap |= bit;
                }
            }
        }

        private static int slot(long key, int shift) {
            return (int) (key >>> shift) & 31;
        }

        private static int differingShift(long a, long b) {
            return ((63 - Long.numberOfLeadingZeros(a ^ b)) / 5) * 5;
        }

        private abstract static class Leaf extends Node {
            Leaf(long key, Object owner) {
                super(key, owner);
            }

            abstract int size();
        }

        private static final class StampLeaf extends Leaf {
            final Keyword label;
            Object version;

            StampLeaf(long key, Keyword label, Object version, Object owner) {
                super(key, owner);
                this.label = label;
                this.version = version;
            }

            int size() {
                return 1;
            }
        }

        private static final class ManyLeaf extends Leaf {
            Labels labels;
            int count;

            ManyLeaf(long key, Labels labels, int count, Object owner) {
                super(key, owner);
                this.labels = labels;
                this.count = count;
            }

            int size() {
                return count;
            }
        }

        private static Leaf find(Node node, long key) {
            while (node instanceof Branch) {
                Branch branch = (Branch) node;
                if (!branch.matches(key)) {
                    return null;
                }
                node = branch.child(slot(key, branch.shift));
            }
            return node != null && node.key == key ? (Leaf) node : null;
        }

        // Each pair is [keyword, stamp] or [null, child]. No map-entry object per output.
        // bitmap == 0 denotes a full-hash collision bucket (only at shift >= 32).
        private static final class Labels {
            boolean growthTracked;
            final Object owner;
            int bitmap;
            Object[] entries;

            Labels(Object owner, int bitmap, Object[] entries) {
                this.owner = owner;
                this.bitmap = bitmap;
                this.entries = entries;
            }

            Labels editable(Object owner) {
                return this.owner == owner ? this : new Labels(owner, bitmap, entries.clone());
            }

            int size() {
                return bitmap == 0 ? entries.length / 2 : Integer.bitCount(bitmap);
            }

            int index(Keyword label, int shift) {
                if (shift >= 32) {
                    for (int i = 0; i < size() * 2; i += 2) {
                        if (entries[i] == label) {
                            return i / 2;
                        }
                    }
                    return -1;
                }
                int bit = 1 << ((label.hasheq() >>> shift) & 31);
                return (bitmap & bit) == 0 ? -1 : Integer.bitCount(bitmap & (bit - 1));
            }

            Labels insert(Editor edit, Keyword label, Object stamp, int shift) {
                int index = index(label, shift);
                if (index < 0) {
                    int size = size();
                    int capacity = entries.length / 2;
                    int grown =
                            shift >= 32
                                    ? size + 1
                                    : size == capacity ? Math.max(2, capacity * 2) : capacity;
                    int bit = shift >= 32 ? 0 : 1 << ((label.hasheq() >>> shift) & 31);
                    index = shift >= 32 ? size : Integer.bitCount(bitmap & (bit - 1));
                    Labels result;
                    if (owner != edit.owner) {
                        Object[] added = new Object[grown * 2];
                        System.arraycopy(entries, 0, added, 0, index * 2);
                        System.arraycopy(
                                entries, index * 2, added, index * 2 + 2, (size - index) * 2);
                        result = new Labels(edit.owner, bitmap, added);
                    } else {
                        result = this;
                        if (grown != capacity) {
                            result.entries = Arrays.copyOf(entries, grown * 2);
                        }
                        System.arraycopy(
                                result.entries,
                                index * 2,
                                result.entries,
                                index * 2 + 2,
                                (size - index) * 2);
                    }
                    result.entries[index * 2] = label;
                    result.entries[index * 2 + 1] = stamp;
                    if (grown != capacity && grown > size + 1) {
                        edit.trackGrowth(result);
                    }
                    result.bitmap |= bit;
                    ++edit.outputCount;
                    return result;
                }

                Labels result = editable(edit.owner);
                Object existingLabel = result.entries[index * 2];
                if (existingLabel == label) {
                    result.entries[index * 2 + 1] = stamp;
                } else if (existingLabel == null) {
                    result.entries[index * 2 + 1] =
                            ((Labels) result.entries[index * 2 + 1])
                                    .insert(edit, label, stamp, shift + 5);
                } else {
                    Keyword other = (Keyword) existingLabel;
                    Labels child =
                            pair(
                                    edit.owner,
                                    other,
                                    result.entries[index * 2 + 1],
                                    label,
                                    stamp,
                                    shift + 5);
                    ++edit.outputCount;
                    result.entries[index * 2] = null;
                    result.entries[index * 2 + 1] = child;
                }
                return result;
            }

            static Labels pair(
                    Object owner,
                    Keyword first,
                    Object firstStamp,
                    Keyword second,
                    Object secondStamp,
                    int shift) {
                if (shift >= 32) {
                    return new Labels(
                            owner, 0, new Object[] {first, firstStamp, second, secondStamp});
                }
                int firstSlot = (first.hasheq() >>> shift) & 31;
                int secondSlot = (second.hasheq() >>> shift) & 31;
                int bitmap = (1 << firstSlot) | (1 << secondSlot);
                if (firstSlot == secondSlot) {
                    return new Labels(
                            owner,
                            bitmap,
                            new Object[] {
                                null, pair(owner, first, firstStamp, second, secondStamp, shift + 5)
                            });
                }
                return new Labels(
                        owner,
                        bitmap,
                        firstSlot < secondSlot
                                ? new Object[] {first, firstStamp, second, secondStamp}
                                : new Object[] {second, secondStamp, first, firstStamp});
            }
        }

        private static Object stamp(Leaf leaf, Keyword label) {
            if (leaf == null) {
                return null;
            }
            if (leaf instanceof StampLeaf) {
                return ((StampLeaf) leaf).label == label ? ((StampLeaf) leaf).version : null;
            }
            Labels labels = ((ManyLeaf) leaf).labels;
            for (int shift = 0; ; shift += 5) {
                int index = labels.index(label, shift);
                if (index < 0) {
                    return null;
                }
                Object key = labels.entries[index * 2];
                if (key != null) {
                    return key == label ? labels.entries[index * 2 + 1] : null;
                }
                labels = (Labels) labels.entries[index * 2 + 1];
            }
        }

        private static final class Editor {
            private final Outputs source;
            private final Object owner = new Object();
            private final Object stamp;
            private Node root;
            private int nodeCount;
            private int outputCount;
            private boolean modified;
            private long lastKey;
            private Leaf lastLeaf;
            private final InvalidationCounters.Editor live;
            private ArrayList<Labels> grownLabels;

            private void trackGrowth(Labels labels) {
                if (labels.growthTracked) {
                    return;
                }
                labels.growthTracked = true;
                if (grownLabels == null) {
                    grownLabels = new ArrayList<>();
                }
                grownLabels.add(labels);
            }

            private Editor(Outputs source, InvalidationCounters.Editor live) {
                this.source = source;
                this.root = source.root;
                this.nodeCount = source.nodeCount;
                this.outputCount = source.outputCount;
                this.stamp = new Object();
                this.live = live;
            }

            void bump(long nodeId, Keyword label) {
                bumpImpl(nodeId, label);
            }

            private void bumpImpl(long nodeId, Keyword label) {
                // A single-entry cursor handles consecutive outputs without any
                // temporary index. Changing leaf representation goes through the trie.
                if (lastLeaf != null && lastKey == nodeId && lastLeaf instanceof ManyLeaf) {
                    ManyLeaf leaf = (ManyLeaf) lastLeaf;
                    int before = outputCount;
                    leaf.labels = leaf.labels.insert(this, label, stamp, 0);
                    leaf.count += outputCount - before;
                } else {
                    lastLeaf = null;
                    root = insert(root, nodeId, label);
                }
                if (lastLeaf != null) {
                    modified = true;
                }
            }

            private Leaf first(long key, Keyword label) {
                if (!live.contains(key)) {
                    return null;
                }
                ++nodeCount;
                ++outputCount;
                return new StampLeaf(key, label, stamp, owner);
            }

            private Node insert(Node node, long key, Keyword label) {
                if (node == null) {
                    return remember(first(key, label));
                }
                if (node instanceof Branch) {
                    Branch branch = (Branch) node;
                    if (!branch.matches(key)) {
                        Leaf added = remember(first(key, label));
                        return added == null
                                ? node
                                : join(node, added, differingShift(key, node.key));
                    }
                    int index = slot(key, branch.shift);
                    Node oldChild = branch.child(index);
                    Node child = insert(oldChild, key, label);
                    if (child == oldChild) {
                        return branch;
                    }
                    Branch result = branch.editable(owner);
                    result.setChild(index, child);
                    return result;
                }
                if (node.key != key) {
                    Leaf added = remember(first(key, label));
                    return added == null ? node : join(node, added, differingShift(key, node.key));
                }

                if (node instanceof StampLeaf) {
                    StampLeaf leaf = (StampLeaf) node;
                    if (leaf.label == label) {
                        if (leaf.owner != owner) {
                            return remember(new StampLeaf(key, label, stamp, owner));
                        }
                        leaf.version = stamp;
                        return remember(leaf);
                    }
                    Labels labels = Labels.pair(owner, leaf.label, leaf.version, label, stamp, 0);
                    ++outputCount;
                    return remember(new ManyLeaf(key, labels, 2, owner));
                }
                ManyLeaf leaf = (ManyLeaf) node;
                ManyLeaf result =
                        leaf.owner == owner
                                ? leaf
                                : new ManyLeaf(key, leaf.labels, leaf.count, owner);
                int before = outputCount;
                result.labels = leaf.labels.insert(this, label, stamp, 0);
                result.count += outputCount - before;
                return remember(result);
            }

            private Leaf remember(Leaf leaf) {
                if (leaf == null) {
                    return null;
                }
                lastKey = leaf.key;
                lastLeaf = leaf;
                return leaf;
            }

            private Branch join(Node old, Node added, int shift) {
                int oldSlot = slot(old.key, shift);
                int addedSlot = slot(added.key, shift);
                Node[] children =
                        oldSlot < addedSlot ? new Node[] {old, added} : new Node[] {added, old};
                return new Branch(
                        old.key, shift, children, (1 << oldSlot) | (1 << addedSlot), owner);
            }

            void remove(long nodeId) {
                lastLeaf = null;
                root = remove(root, nodeId);
            }

            private Node remove(Node node, long key) {
                if (node instanceof Branch) {
                    Branch branch = (Branch) node;
                    if (!branch.matches(key)) {
                        return branch;
                    }
                    int index = slot(key, branch.shift);
                    Node oldChild = branch.child(index);
                    Node child = remove(oldChild, key);
                    if (child == oldChild) {
                        return branch;
                    }
                    Branch result = branch.editable(owner);
                    result.setChild(index, child);
                    if (Integer.bitCount(result.bitmap) == 1) {
                        for (Node remaining : result.children) {
                            if (remaining != null) {
                                return remaining;
                            }
                        }
                    }
                    return result;
                }
                if (node != null && node.key == key) {
                    --nodeCount;
                    outputCount -= ((Leaf) node).size();
                    modified = true;
                    return null;
                }
                return node;
            }

            public Outputs finish() {
                if (!modified) {
                    return source;
                }
                if (root == null) {
                    return source.clear();
                }
                if (grownLabels != null) {
                    for (Labels labels : grownLabels) {
                        if (labels.entries.length > labels.size() * 2) {
                            labels.entries = Arrays.copyOf(labels.entries, labels.size() * 2);
                        }
                    }
                    grownLabels = null;
                }
                return new Outputs(root, nodeCount, outputCount);
            }
        }
    }
}
