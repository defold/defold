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
import clojure.lang.IKVReduce;
import clojure.lang.IPersistentMap;
import clojure.lang.Keyword;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReferenceArray;

/// Caches which endpoints depend directly on a given node and label.
///
/// After an edit, the old and new graph versions can share cache entries the
/// edit didn't affect. Invalidated entries need separate storage, even when
/// they're empty. A thread can still fill an empty entry in the old version
/// after the edit, and its answer belongs to that version.
///
/// Cache lookups run without taking the writer's lock. The writer stores
/// the array before making the output label visible. Once a lookup finds
/// the label, it can safely read the array. Treat stored arrays as read-only.
///
/// An endpoint is identified by its node ID and label. Two graph versions
/// can cache different `Endpoint` instances for the same pair. Use `equals`
/// to check whether they represent the same endpoint.
public final class Successors {
    private static final int PAGE_BITS = 6;
    private static final int PAGE_SIZE = 1 << PAGE_BITS;
    private static final int PAGE_MASK = PAGE_SIZE - 1;
    private static final int BLOCK_BITS = 4;
    private static final int BLOCK_SIZE = 1 << BLOCK_BITS;
    private static final int BLOCK_MASK = BLOCK_SIZE - 1;
    private static final int BLOCK_COUNT = PAGE_SIZE / BLOCK_SIZE;
    private static final long BLOCK_MASK_BITS = (1L << BLOCK_SIZE) - 1;
    private static final int BRANCH_BITS = 5;
    private static final int BRANCH_SIZE = 1 << BRANCH_BITS;
    private static final int BRANCH_MASK = BRANCH_SIZE - 1;

    private volatile Directory directory;

    public Successors() {
        this(Directory.EMPTY);
    }

    private Successors(Directory directory) {
        this.directory = directory;
    }

    public Endpoint endpoint(long nodeId, Keyword label) {
        return endpointCell(nodeId).intern(nodeId, label);
    }

    public void appendEndpoints(ArrayList<Endpoint> target, long nodeId, Set<Keyword> labels) {
        if (labels != null && !labels.isEmpty()) {
            EndpointCell cell = endpointCell(nodeId);
            for (Keyword label : labels) {
                target.add(cell.intern(nodeId, label));
            }
        }
    }

    private EndpointCell endpointCell(long nodeId) {
        return pageForWrite(nodeId >>> PAGE_BITS).endpoints.cell((int) nodeId & PAGE_MASK);
    }

    /**
     * Returns the cached successors, or {@code null} on a cache miss.
     * A cached output with no successors has an empty array.
     */
    public Endpoint[] get(long nodeId, Keyword label) {
        Page page = directory.get(nodeId >>> PAGE_BITS);
        AtomicReferenceArray<RowCell> rows = page == null ? null : page.rows;
        if (rows == null) {
            return null;
        }
        RowCell node = rows.getAcquire((int) nodeId & PAGE_MASK);
        return node == null ? null : node.get(label);
    }

    /** Returns the existing row, or {@code null} if this call stored the supplied row. */
    public Endpoint[] putIfAbsent(long nodeId, Keyword label, Endpoint[] endpoints) {
        int slot = (int) nodeId & PAGE_MASK;
        Page page = pageForWrite(nodeId >>> PAGE_BITS);
        AtomicReferenceArray<RowCell> rows = page.rows;
        if (rows == null) {
            rows = page.installRows();
        }
        RowCell node = rows.getAcquire(slot);
        if (node == null) {
            RowCell proposed = new RowCell(null, 0);
            node = rows.compareAndExchange(slot, null, proposed);
            if (node == null) {
                node = proposed;
            }
        }
        return node.putIfAbsent(label, endpoints);
    }

    private Page pageForWrite(long pageId) {
        Page page = directory.get(pageId);
        return page == null ? installPage(pageId) : page;
    }

    private synchronized Page installPage(long pageId) {
        Directory before = directory;
        Page page = before.get(pageId);
        if (page == null) {
            page = new Page(new EndpointPage(null), null);
            directory = before.with(pageId, page);
        }
        return page;
    }

    /**
     * Returns the cache for the basis after a transaction. A null label set
     * invalidates every row for that node. The new basis's node map tells
     * whether the node is still present.
     */
    public Successors invalidate(IPersistentMap changes, IPersistentMap nodes) {
        if (changes.count() == 0) {
            return this;
        }

        Editor editor = new Editor(directory);
        ((IKVReduce) changes).kvreduce(new AFn() {
            @Override
            public Object invoke(Object accumulator, Object key, Object labels) {
                long nodeId = ((Number) key).longValue();
                editor.invalidate(nodeId, (Set<?>) labels, labels == null && !nodes.containsKey(key));
                return accumulator;
            }
        }, null);
        return new Successors(editor.finish());
    }

    private static final class Page {
        private final EndpointPage endpoints;
        private volatile AtomicReferenceArray<RowCell> rows;

        private Page(EndpointPage endpoints, AtomicReferenceArray<RowCell> rows) {
            this.endpoints = endpoints;
            this.rows = rows;
        }

        private synchronized AtomicReferenceArray<RowCell> installRows() {
            if (rows == null) {
                rows = new AtomicReferenceArray<>(PAGE_SIZE);
            }
            return rows;
        }
    }

    private static final class EndpointPage {
        private volatile AtomicReferenceArray<AtomicReferenceArray<EndpointCell>> blocks;

        private EndpointPage(AtomicReferenceArray<AtomicReferenceArray<EndpointCell>> blocks) {
            this.blocks = blocks;
        }

        private EndpointCell cell(int slot) {
            return endpointCell(block(slot >>> BLOCK_BITS), slot & BLOCK_MASK);
        }

        private AtomicReferenceArray<EndpointCell> block(int index) {
            AtomicReferenceArray<AtomicReferenceArray<EndpointCell>> array = blocks;
            if (array == null) {
                array = installBlocks();
            }
            AtomicReferenceArray<EndpointCell> existing = array.getAcquire(index);
            if (existing != null) {
                return existing;
            }
            AtomicReferenceArray<EndpointCell> proposed = new AtomicReferenceArray<>(BLOCK_SIZE);
            existing = array.compareAndExchange(index, null, proposed);
            return existing == null ? proposed : existing;
        }

        private synchronized AtomicReferenceArray<AtomicReferenceArray<EndpointCell>> installBlocks() {
            if (blocks == null) {
                blocks = new AtomicReferenceArray<>(BLOCK_COUNT);
            }
            return blocks;
        }

        private EndpointPage without(long deleted) {
            if (deleted == 0) {
                return this;
            }
            AtomicReferenceArray<AtomicReferenceArray<EndpointCell>> previous = blocks;
            if (previous == null) {
                return null;
            }
            AtomicReferenceArray<AtomicReferenceArray<EndpointCell>> retained = null;
            for (int index = 0; index < BLOCK_COUNT; index++) {
                AtomicReferenceArray<EndpointCell> block = previous.getAcquire(index);
                long removed = (deleted >>> (index * BLOCK_SIZE)) & BLOCK_MASK_BITS;
                if (block != null && removed != 0) {
                    AtomicReferenceArray<EndpointCell> copy = null;
                    for (long remaining = BLOCK_MASK_BITS ^ removed; remaining != 0; remaining &= remaining - 1) {
                        int slot = Long.numberOfTrailingZeros(remaining);
                        EndpointCell cell = block.getAcquire(slot);
                        if (cell != null) {
                            if (copy == null) {
                                copy = new AtomicReferenceArray<>(BLOCK_SIZE);
                            }
                            copy.setPlain(slot, cell);
                        }
                    }
                    block = copy;
                }
                if (block != null) {
                    if (retained == null) {
                        retained = new AtomicReferenceArray<>(BLOCK_COUNT);
                    }
                    retained.setPlain(index, block);
                }
            }
            return retained == null ? null : new EndpointPage(retained);
        }
    }

    private static EndpointCell endpointCell(AtomicReferenceArray<EndpointCell> cells, int slot) {
        EndpointCell existing = cells.getAcquire(slot);
        if (existing != null) {
            return existing;
        }
        EndpointCell proposed = new EndpointCell();
        existing = cells.compareAndExchange(slot, null, proposed);
        return existing == null ? proposed : existing;
    }

    private static final class Editor {
        private final Directory previous;
        private HashMap<Long, Draft> pages;
        private Draft last;

        private Editor(Directory previous) {
            this.previous = previous;
        }

        private Draft draft(long pageId) {
            if (last == null) {
                last = new Draft(pageId, previous.get(pageId));
            } else if (last.pageId != pageId) {
                if (pages == null) {
                    pages = new HashMap<>();
                    pages.put(last.pageId, last);
                }
                last = pages.computeIfAbsent(pageId, id -> new Draft(id, previous.get(id)));
            }
            return last;
        }

        private void invalidate(long nodeId, Set<?> labels, boolean deleted) {
            Draft draft = draft(nodeId >>> PAGE_BITS);
            int slot = (int) nodeId & PAGE_MASK;
            if (deleted) {
                draft.deleted |= 1L << slot;
            }
            draft.invalidateRows(slot, labels);
        }

        private Directory finish() {
            if (pages == null) {
                return last == null ? previous : replace(previous, last);
            }
            Directory updated = previous;
            for (Draft draft : pages.values()) {
                updated = replace(updated, draft);
            }
            return updated;
        }

        private static Directory replace(Directory directory, Draft draft) {
            Page page = draft.finish();
            return page == draft.previous ? directory : directory.with(draft.pageId, page);
        }
    }

    private static final class Draft {
        private final long pageId;
        private final Page previous;
        private long deleted;
        private AtomicReferenceArray<RowCell> rows;
        private boolean copiedRows;

        private Draft(long pageId, Page previous) {
            this.pageId = pageId;
            this.previous = previous;
            this.rows = previous == null ? null : previous.rows;
        }

        private void invalidateRows(int slot, Set<?> labels) {
            if (rows != null) {
                // A worker on the old basis can still publish a row that is missing here.
                if (!copiedRows) {
                    AtomicReferenceArray<RowCell> copy = new AtomicReferenceArray<>(PAGE_SIZE);
                    for (int i = 0; i < PAGE_SIZE; i++) {
                        copy.setPlain(i, rows.getAcquire(i));
                    }
                    rows = copy;
                    copiedRows = true;
                }
                RowCell node = rows.getPlain(slot);
                rows.setPlain(slot, labels == null || node == null ? null : node.without(labels));
            }
        }

        private Page finish() {
            if (previous == null) {
                return null;
            }
            if (allEmpty(rows)) {
                rows = null;
            }
            EndpointPage endpoints = previous.endpoints.without(deleted);
            if (endpoints == null) {
                if (rows == null) {
                    return null;
                }
                endpoints = new EndpointPage(null);
            }
            // Even a page with no rows yet needs its own row storage after invalidation.
            return new Page(endpoints, rows);
        }
    }

    private static boolean allEmpty(AtomicReferenceArray<RowCell> values) {
        if (values != null) {
            for (int index = 0; index < values.length(); index++) {
                if (values.getPlain(index) != null) {
                    return false;
                }
            }
        }
        return true;
    }

    private static final class RowCell {
        // Each key is followed by its successor array. Writers publish the key last.
        private volatile AtomicReferenceArray<Object> rows;
        private int size;

        private RowCell(AtomicReferenceArray<Object> rows, int size) {
            this.rows = rows;
            this.size = size;
        }

        private Endpoint[] get(Keyword label) {
            AtomicReferenceArray<Object> table = rows;
            if (table != null) {
                int mask = table.length() - 1;
                int index = (label.hasheq() << 1) & mask;
                while (true) {
                    Object key = table.getAcquire(index);
                    if (key == label) {
                        return (Endpoint[]) table.getPlain(index + 1);
                    }
                    if (key == null) {
                        break;
                    }
                    index = (index + 2) & mask;
                }
            }
            return null;
        }

        private synchronized Endpoint[] putIfAbsent(Keyword label, Endpoint[] row) {
            AtomicReferenceArray<Object> table = rows;
            int index = table == null ? 0 : slot(table, label);
            if (table != null && table.getPlain(index) != null) {
                return (Endpoint[]) table.getPlain(index + 1);
            }
            if (table == null || !fits(size + 1, table.length())) {
                AtomicReferenceArray<Object> grown = new AtomicReferenceArray<>(table == null ? 4 : table.length() * 2);
                if (table != null) {
                    copy(table, grown, Set.of());
                }
                insert(grown, slot(grown, label), label, row);
                rows = grown;
            } else {
                insert(table, index, label, row);
            }
            size++;
            return null;
        }

        private synchronized RowCell without(Set<?> labels) {
            AtomicReferenceArray<Object> table = rows;
            if (table == null) {
                return null;
            }
            int kept = size;
            for (Object label : labels) {
                if (table.getPlain(slot(table, (Keyword) label)) != null) {
                    kept--;
                }
            }
            if (kept == 0) {
                return null;
            }
            int length = table.length();
            while (fits(kept, length / 2)) {
                length /= 2;
            }
            AtomicReferenceArray<Object> retained = new AtomicReferenceArray<>(length);
            copy(table, retained, labels);
            return new RowCell(retained, kept);
        }

        private static boolean fits(int size, int length) {
            // At most three quarters of the key slots can be occupied.
            return size * 8L <= length * 3L;
        }

        private static int slot(AtomicReferenceArray<Object> table, Keyword label) {
            int mask = table.length() - 1;
            int index = (label.hasheq() << 1) & mask;
            Object key;
            while ((key = table.getPlain(index)) != null && key != label) {
                index = (index + 2) & mask;
            }
            return index;
        }

        private static void insert(AtomicReferenceArray<Object> table, int index, Keyword label, Endpoint[] row) {
            table.setPlain(index + 1, row);
            table.setRelease(index, label);
        }

        private static void copy(AtomicReferenceArray<Object> source, AtomicReferenceArray<Object> target, Set<?> removed) {
            for (int index = 0; index < source.length(); index += 2) {
                Keyword label = (Keyword) source.getPlain(index);
                if (label != null && !removed.contains(label)) {
                    insert(target, slot(target, label), label, (Endpoint[]) source.getPlain(index + 1));
                }
            }
        }
    }

    private static final class EndpointCell {
        private volatile Object values;
        private int count;

        private Endpoint intern(long nodeId, Keyword label) {
            Object before = values;
            if (before instanceof Endpoint endpoint && endpoint.label() == label) {
                return endpoint;
            }
            if (before instanceof AtomicReferenceArray<?> table) {
                int mask = table.length() - 1;
                int slot = label.hasheq() & mask;
                while (true) {
                    Endpoint endpoint = (Endpoint) table.getAcquire(slot);
                    if (endpoint == null) {
                        break;
                    }
                    if (endpoint.label() == label) {
                        return endpoint;
                    }
                    slot = (slot + 1) & mask;
                }
            }
            return insert(nodeId, label);
        }

        private synchronized Endpoint insert(long nodeId, Keyword label) {
            Object before = values;
            if (before == null) {
                Endpoint endpoint = new Endpoint(nodeId, label);
                values = endpoint;
                count = 1;
                return endpoint;
            }
            if (before instanceof Endpoint one) {
                if (one.label() == label) {
                    return one;
                }
                AtomicReferenceArray<Endpoint> table = new AtomicReferenceArray<>(4);
                Endpoint endpoint = new Endpoint(nodeId, label);
                table.setPlain(slot(table, one.label()), one);
                table.setPlain(slot(table, label), endpoint);
                count = 2;
                values = table;
                return endpoint;
            }

            @SuppressWarnings("unchecked")
            AtomicReferenceArray<Endpoint> table = (AtomicReferenceArray<Endpoint>) before;
            int slot = slot(table, label);
            Endpoint existing = table.getPlain(slot);
            if (existing != null) {
                return existing;
            }
            Endpoint endpoint = new Endpoint(nodeId, label);
            if ((count + 1) * 4 <= table.length() * 3) {
                table.setRelease(slot, endpoint);
            } else {
                AtomicReferenceArray<Endpoint> grown = new AtomicReferenceArray<>(table.length() * 2);
                for (int index = 0; index < table.length(); index++) {
                    Endpoint retained = table.getPlain(index);
                    if (retained != null) {
                        grown.setPlain(slot(grown, retained.label()), retained);
                    }
                }
                grown.setPlain(slot(grown, label), endpoint);
                values = grown;
            }
            count++;
            return endpoint;
        }

        private static int slot(AtomicReferenceArray<Endpoint> table, Keyword label) {
            int mask = table.length() - 1;
            int slot = label.hasheq() & mask;
            while (table.getPlain(slot) != null && table.getPlain(slot).label() != label) {
                slot = (slot + 1) & mask;
            }
            return slot;
        }
    }

    private static final class Directory {
        private static final Directory EMPTY = new Directory(null, 0);
        private final Object[] root;
        private final int shift;
        private Directory(Object[] root, int shift) {
            this.root = root;
            this.shift = shift;
        }

        private Page get(long pageId) {
            Object[] branch = root;
            if (branch == null || (pageId >>> (shift + BRANCH_BITS)) != 0) {
                return null;
            }
            for (int level = shift; level > 0; level -= BRANCH_BITS) {
                branch = (Object[]) branch[(int) (pageId >>> level) & BRANCH_MASK];
                if (branch == null) {
                    return null;
                }
            }
            return (Page) branch[(int) pageId & BRANCH_MASK];
        }

        private Directory with(long pageId, Page page) {
            Object[] updated = root;
            int updatedShift = shift;
            while ((pageId >>> (updatedShift + BRANCH_BITS)) != 0) {
                Object[] parent = new Object[BRANCH_SIZE];
                parent[0] = updated;
                updated = parent;
                updatedShift += BRANCH_BITS;
            }
            updated = replace(updated, updatedShift, pageId, page);
            while (updatedShift > 0 && onlyFirstChild(updated)) {
                updated = updated == null ? null : (Object[]) updated[0];
                updatedShift -= BRANCH_BITS;
            }
            return updated == null ? EMPTY : new Directory(updated, updatedShift);
        }

        private static boolean onlyFirstChild(Object[] branch) {
            if (branch != null) {
                for (int index = 1; index < branch.length; index++) {
                    if (branch[index] != null) {
                        return false;
                    }
                }
            }
            return true;
        }

        private static Object[] replace(Object[] branch, int shift, long pageId, Page page) {
            Object[] copy = branch == null ? new Object[BRANCH_SIZE] : branch.clone();
            int index = (int) (pageId >>> shift) & BRANCH_MASK;
            copy[index] = shift == 0 ? page : replace((Object[]) copy[index], shift - BRANCH_BITS, pageId, page);
            return page == null && copy[0] == null && onlyFirstChild(copy) ? null : copy;
        }

    }
}
