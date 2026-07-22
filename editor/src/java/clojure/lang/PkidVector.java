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

package clojure.lang;

import clojure.data.int_map.IntSet;
import clojure.data.int_map.IntSet.BitSetContainer;
import clojure.data.int_map.IntSet.SingleContainer;
import clojure.data.int_map.INode;

import java.io.Serial;
import java.util.Arrays;
import java.util.BitSet;
import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * A persistent vector whose realized indexes are separate from its stable
 * pkids.
 *
 * <p>This class lives in {@code clojure.lang} because {@link PersistentVector}
 * exposes only package-private constructors. Normal vector operations use
 * realized indexes, and vector equality and hashing consider only realized
 * values.</p>
 *
 * <p>Clojure's transient vector implementation is final and cannot retain the
 * additional fields, so generic transient conversion is rejected.
 * Consequently, this class cannot be used as the target of
 * transient-optimized operations such as {@code into}. This class also does
 * not define a Java serialization format for its pkid state.</p>
 */
public final class PkidVector extends PersistentVector {
    @Serial
    private static final long serialVersionUID = 1L;

    private static final short MISSING_PKID_LEAF_SIZE = 128;
    private static final long[] EMPTY_PKIDS = new long[0];

    public static final PkidVector EMPTY = new PkidVector(PersistentVector.EMPTY, new IntSet(MISSING_PKID_LEAF_SIZE), 0, 0);

    private final IntSet missingPkids;
    private final long missingPkidsEpoch;
    public final long nextPkid;

    private PkidVector(final PersistentVector values, final IntSet missingPkids, final long missingPkidsEpoch, final long nextPkid) {
        this(values._meta, values.cnt, values.shift, values.root, values.tail, missingPkids, missingPkidsEpoch, nextPkid);
    }

    private PkidVector(final IPersistentMap meta, final int count, final int shift, final Node root, final Object[] tail, final IntSet missingPkids, final long missingPkidsEpoch, final long nextPkid) {
        super(meta, count, shift, root, tail);
        this.missingPkids = missingPkids;
        this.missingPkidsEpoch = missingPkidsEpoch;
        this.nextPkid = nextPkid;
    }

    private static PkidVector fromTransient(final ITransientVector transientValues, final IPersistentMap meta, final IntSet missingPkids, final long missingPkidsEpoch, final long nextPkid) {
        final TransientVector transientVector = (TransientVector) transientValues;
        transientVector.ensureEditable();
        transientVector.root.edit.set(null);

        final int count = transientVector.cnt;
        final int tailOffset = count < 32 ? 0 : ((count - 1) >>> 5) << 5;
        final Object[] tail = new Object[count - tailOffset];
        System.arraycopy(transientVector.tail, 0, tail, 0, tail.length);

        return new PkidVector(meta, count, transientVector.shift, transientVector.root, tail, missingPkids, missingPkidsEpoch, nextPkid);
    }

    private static long checkedPkid(final long pkid) {
        if (pkid < 0) {
            throw new IllegalArgumentException("pkid must not be negative: " + pkid);
        }

        return pkid;
    }

    private static int initialPkidCapacity(final Object pkids) {
        if (pkids instanceof Counted) {
            return Math.min(((Counted) pkids).count(), 8);
        }

        return pkids == null ? 0 : 8;
    }

    private static ITransientVector conjValues(final ITransientVector result, final PersistentVector values, final int startIndex, final int endIndex) {
        ITransientVector newResult = result;
        int index = startIndex;

        while (index < endIndex) {
            final Object[] array = values.arrayFor(index);
            int offset = index & 0x01f;
            final int limit = Math.min(array.length, offset + endIndex - index);

            while (offset < limit) {
                newResult = (ITransientVector) newResult.conj(array[offset]);
                ++offset;
                ++index;
            }
        }

        return newResult;
    }

    private static IntSet addPkidRange(IntSet result, final long epoch, final long firstPkid, final long endPkid) {
        for (long pkid = firstPkid; pkid < endPkid; ++pkid) {
            result = (IntSet) result.add(epoch, pkid);
        }

        return result;
    }

    private static int binarySearchHeight(long itemCount) {
        int height = 0;

        while (itemCount > 1) {
            itemCount -= itemCount / 2;
            ++height;
        }

        return height;
    }

    private long missingPkidCountInRange(final long endPkid) {
        if (endPkid == 0) {
            return 0;
        }

        final long rangeEpoch = missingPkidsEpoch + 1;
        final IntSet range = (IntSet) missingPkids.range(rangeEpoch, 0, endPkid - 1);
        return range.count();
    }

    private long pkidAtIndex(final long index) {
        long low = index;
        long high = nextPkid - 1;

        while (low != high) {
            final long mid = low + (high - low) / 2;
            final long missingCountThroughMid = missingPkidCountInRange(mid + 1);
            final long liveCountThroughMid = mid + 1 - missingCountThroughMid;

            if (index < liveCountThroughMid) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }

        return low;
    }

    private static final class PkidBatch {
        private long[] pkids;
        private int count;
        private boolean sorted = true;
        private boolean restoresMissing;

        PkidBatch(final int initialCapacity) {
            pkids = initialCapacity == 0 ? EMPTY_PKIDS : new long[initialCapacity];
        }

        void add(final long pkid) {
            if (count != 0 && pkid < pkids[count - 1]) {
                sorted = false;
            }

            if (count == pkids.length) {
                final int newCapacity = count < 8 ? 8 : Math.multiplyExact(count, 2);
                pkids = Arrays.copyOf(pkids, newCapacity);
            }

            pkids[count] = pkid;
            ++count;
        }

        void finish() {
            if (count < 2) {
                return;
            }

            if (!sorted) {
                Arrays.sort(pkids, 0, count);
            }

            int uniqueCount = 1;
            for (int index = 1; index < count; ++index) {
                final long pkid = pkids[index];
                if (pkid != pkids[uniqueCount - 1]) {
                    pkids[uniqueCount] = pkid;
                    ++uniqueCount;
                }
            }

            count = uniqueCount;
        }
    }

    private static final class MissingPkidCursor implements Iterator<Long> {
        private final Iterator<?> leafIterator;
        private BitSet bitSet;
        private long leafBase;
        private int bitIndex;
        private boolean hasValue;
        private long value;
        private long countBefore;

        MissingPkidCursor(final IntSet missingPkids) {
            leafIterator = missingPkids.map.iterator(INode.IterationType.ENTRIES, false);
            advance();
        }

        @Override
        public boolean hasNext() {
            return hasValue;
        }

        @Override
        public Long next() {
            return nextLong();
        }

        long nextLong() {
            if (!hasValue) {
                throw new NoSuchElementException();
            }

            final long result = value;
            advance();
            return result;
        }

        long countBefore(final long pkid) {
            while (hasValue && value < pkid) {
                ++countBefore;
                advance();
            }

            return countBefore;
        }

        boolean isAt(final long pkid) {
            return hasValue && value == pkid;
        }

        private void advance() {
            if (bitSet != null) {
                final int nextBitIndex = bitSet.nextSetBit(bitIndex + 1);
                if (nextBitIndex >= 0) {
                    bitIndex = nextBitIndex;
                    value = leafBase + bitIndex;
                    return;
                }

                bitSet = null;
            }

            while (leafIterator.hasNext()) {
                final MapEntry entry = (MapEntry) leafIterator.next();
                final long leafIndex = (long) entry.key();
                final Object container = entry.val();
                leafBase = leafIndex * MISSING_PKID_LEAF_SIZE;

                switch (container) {
                    case null -> {
                        continue;
                    }
                    case SingleContainer singleContainer -> {
                        value = leafBase + singleContainer.val;
                        hasValue = true;
                        return;
                    }
                    case BitSetContainer bitSetContainer -> {
                        bitSet = bitSetContainer.bitSet;
                        bitIndex = bitSet.nextSetBit(0);
                        if (bitIndex >= 0) {
                            value = leafBase + bitIndex;
                            hasValue = true;
                            return;
                        }

                        bitSet = null;
                        continue;
                    }
                    default -> {
                    }
                }

                final String containerClass = container.getClass().getName();
                throw new IllegalStateException("Unsupported IntSet container: " + containerClass);
            }

            hasValue = false;
        }
    }

    private void intoBatch(Object pkids, PkidBatch batch) {
        if (pkids instanceof IReduceInit) {
            ((IReduceInit) pkids).reduce(new AFn() {
                @Override
                public Object invoke(final Object result, final Object value) {
                    final long pkid = (long) value;
                    batch.add(checkedPkid(pkid));
                    return result;
                }
            }, null);
        } else {
            final Iterator<?> iterator = RT.iter(pkids);

            while (iterator.hasNext()) {
                final long pkid = (long) iterator.next();
                batch.add(checkedPkid(pkid));
            }
        }

        batch.finish();
    }

    private PkidBatch normalizeAssocPkids(final Object pkids) {
        final boolean hasMissingPkids = nextPkid != cnt;
        final PkidBatch batch = new PkidBatch(initialPkidCapacity(pkids));
        intoBatch(pkids, batch);

        if (hasMissingPkids) {
            for (int index = 0; index < batch.count; ++index) {
                final long pkid = batch.pkids[index];
                if (pkid >= nextPkid) {
                    break;
                }

                if (missingPkids.contains(pkid)) {
                    batch.restoresMissing = true;
                    break;
                }
            }
        }

        return batch;
    }

    private PkidBatch normalizeDissocPkids(final Object pkids) {
        final boolean hasMissingPkids = nextPkid != cnt;
        final PkidBatch batch = new PkidBatch(0);
        intoBatch(pkids, batch);
        int livePkidCount = 0;

        for (int index = 0; index < batch.count; ++index) {
            final long pkid = batch.pkids[index];
            if (pkid >= nextPkid) {
                break;
            }

            if (!hasMissingPkids || !missingPkids.contains(pkid)) {
                batch.pkids[livePkidCount] = pkid;
                ++livePkidCount;
            }
        }

        batch.count = livePkidCount;
        return batch;
    }

    private PkidVector assocPkidsWithoutRestoration(final PkidBatch batch, final Object value) {
        final MissingPkidCursor missingCursor = nextPkid == cnt ? null : new MissingPkidCursor(missingPkids);
        ITransientVector newValues = super.asTransient();
        IntSet newMissingPkids = missingPkids;
        long newMissingPkidsEpoch = missingPkidsEpoch;
        long newNextPkid = nextPkid;

        for (int batchIndex = 0; batchIndex < batch.count; ++batchIndex) {
            final long pkid = batch.pkids[batchIndex];

            if (pkid < nextPkid) {
                final long missingCountBefore = missingCursor == null ? 0 : missingCursor.countBefore(pkid);
                final int index = Math.toIntExact(pkid - missingCountBefore);
                newValues = newValues.assocN(index, value);
            } else {
                if (newNextPkid < pkid) {
                    if (newMissingPkidsEpoch == missingPkidsEpoch) {
                        newMissingPkidsEpoch = missingPkidsEpoch + 1;
                    }

                    newMissingPkids = addPkidRange(newMissingPkids, newMissingPkidsEpoch, newNextPkid, pkid);
                }

                newNextPkid = pkid + 1;
                newValues = (ITransientVector) newValues.conj(value);
            }
        }

        return fromTransient(newValues, _meta, newMissingPkids, newMissingPkidsEpoch, newNextPkid);
    }

    private PkidVector assocPkidsWithRestoration(final PkidBatch batch, final Object value) {
        final MissingPkidCursor missingCursor = new MissingPkidCursor(missingPkids);
        long newMissingPkidsEpoch = missingPkidsEpoch + 1;
        ITransientVector newValues = PersistentVector.EMPTY.asTransient();
        IntSet newMissingPkids = missingPkids;
        long newMissingPkidCount = nextPkid - cnt;
        long newNextPkid = nextPkid;
        int sourceIndex = 0;

        for (int batchIndex = 0; batchIndex < batch.count; ++batchIndex) {
            final long pkid = batch.pkids[batchIndex];

            if (pkid < nextPkid) {
                final long missingCountBefore = missingCursor.countBefore(pkid);
                final int index = Math.toIntExact(pkid - missingCountBefore);
                final boolean restoresMissing = missingCursor.isAt(pkid);
                newValues = conjValues(newValues, this, sourceIndex, index);

                if (restoresMissing) {
                    newMissingPkids = (IntSet) newMissingPkids.remove(newMissingPkidsEpoch, pkid);
                    --newMissingPkidCount;
                    sourceIndex = index;
                } else {
                    sourceIndex = index + 1;
                }

                newValues = (ITransientVector) newValues.conj(value);
            } else {
                newValues = conjValues(newValues, this, sourceIndex, cnt);
                sourceIndex = cnt;
                newMissingPkids = addPkidRange(newMissingPkids, newMissingPkidsEpoch, newNextPkid, pkid);
                newMissingPkidCount += pkid - newNextPkid;
                newNextPkid = pkid + 1;
                newValues = (ITransientVector) newValues.conj(value);
            }
        }

        newValues = conjValues(newValues, this, sourceIndex, cnt);
        if (newMissingPkidCount == 0) {
            newMissingPkids = EMPTY.missingPkids;
            newMissingPkidsEpoch = EMPTY.missingPkidsEpoch;
        }

        return fromTransient(newValues, _meta, newMissingPkids, newMissingPkidsEpoch, newNextPkid);
    }

    public PkidVector assocPkids(final Object pkids, final Object value) {
        if (pkids == null || (pkids instanceof Counted && ((Counted) pkids).count() == 0)) {
            return this;
        }

        final PkidBatch batch = normalizeAssocPkids(pkids);
        if (batch.count == 0) {
            return this;
        }

        if (batch.count == 1 && batch.pkids[0] == nextPkid) {
            return cons(value);
        }

        return batch.restoresMissing ? assocPkidsWithRestoration(batch, value) : assocPkidsWithoutRestoration(batch, value);
    }

    public PkidVector dissocPkids(final Object pkids) {
        if (pkids == null || (pkids instanceof Counted && ((Counted) pkids).count() == 0)) {
            return this;
        }

        final PkidBatch batch = normalizeDissocPkids(pkids);
        if (batch.count == 0) {
            return this;
        }

        final MissingPkidCursor missingCursor = nextPkid == cnt ? null : new MissingPkidCursor(missingPkids);
        final long newMissingPkidsEpoch = missingPkidsEpoch + 1;
        ITransientVector newValues = PersistentVector.EMPTY.asTransient();
        IntSet newMissingPkids = missingPkids;
        int sourceIndex = 0;

        for (int batchIndex = 0; batchIndex < batch.count; ++batchIndex) {
            final long pkid = batch.pkids[batchIndex];
            final long missingCountBefore = missingCursor == null ? 0 : missingCursor.countBefore(pkid);
            final int index = Math.toIntExact(pkid - missingCountBefore);
            newValues = conjValues(newValues, this, sourceIndex, index);
            sourceIndex = index + 1;
            newMissingPkids = (IntSet) newMissingPkids.add(newMissingPkidsEpoch, pkid);
        }

        newValues = conjValues(newValues, this, sourceIndex, cnt);
        return fromTransient(newValues, _meta, newMissingPkids, newMissingPkidsEpoch, nextPkid);
    }

    public PersistentVector findPkids(final Object value) {
        final long missingCount = nextPkid - cnt;

        if (missingCount == 0) {
            ITransientVector result = null;
            int index = 0;

            while (index < cnt) {
                final Object[] array = arrayFor(index);
                for (int offset = 0; offset < array.length; ++offset, ++index) {
                    if (Util.equiv(value, array[offset])) {
                        if (result == null) {
                            result = PersistentVector.EMPTY.asTransient();
                        }

                        result = (ITransientVector) result.conj((long) index);
                    }
                }
            }

            return result == null ? PersistentVector.EMPTY : (PersistentVector) result.persistent();
        }

        int[] matchingIndexes = null;
        int matchingIndexCount = 0;
        int index = 0;

        while (index < cnt) {
            final Object[] array = arrayFor(index);
            for (int offset = 0; offset < array.length; ++offset, ++index) {
                if (Util.equiv(value, array[offset])) {
                    if (matchingIndexes == null) {
                        matchingIndexes = new int[Math.min(cnt, 8)];
                    }

                    if (matchingIndexCount == matchingIndexes.length) {
                        final int newCapacity = matchingIndexCount < 8 ? 8 : Math.multiplyExact(matchingIndexCount, 2);
                        matchingIndexes = Arrays.copyOf(matchingIndexes, newCapacity);
                    }

                    matchingIndexes[matchingIndexCount] = index;
                    ++matchingIndexCount;
                }
            }
        }

        if (matchingIndexCount == 0) {
            return PersistentVector.EMPTY;
        }

        ITransientVector result = PersistentVector.EMPTY.asTransient();
        if (missingCount <= (long) matchingIndexCount * binarySearchHeight(nextPkid)) {
            int matchingIndexPosition = 0;
            long missingPkidCount = 0;
            final MissingPkidCursor missingIterator = new MissingPkidCursor(missingPkids);

            while (missingIterator.hasNext() && matchingIndexPosition < matchingIndexCount) {
                final long missingPkid = missingIterator.nextLong();

                while (matchingIndexPosition < matchingIndexCount) {
                    final long pkid = matchingIndexes[matchingIndexPosition] + missingPkidCount;
                    if (pkid >= missingPkid) {
                        break;
                    }

                    result = (ITransientVector) result.conj(pkid);
                    ++matchingIndexPosition;
                }

                ++missingPkidCount;
            }

            while (matchingIndexPosition < matchingIndexCount) {
                final long pkid = matchingIndexes[matchingIndexPosition] + missingPkidCount;
                result = (ITransientVector) result.conj(pkid);
                ++matchingIndexPosition;
            }
        } else {
            for (int matchingIndexPosition = 0; matchingIndexPosition < matchingIndexCount; ++matchingIndexPosition) {
                final long pkid = pkidAtIndex(matchingIndexes[matchingIndexPosition]);
                result = (ITransientVector) result.conj(pkid);
            }
        }

        return (PersistentVector) result.persistent();
    }

    /**
     * Returns a read-only ascending iterator over missing pkids.
     */
    public Iterator<Long> missingPkidIterator() {
        return new MissingPkidCursor(missingPkids);
    }

    @Override
    public PkidVector assocN(final int index, final Object value) {
        if (index == cnt) {
            return cons(value);
        }

        final PersistentVector newValues = super.assocN(index, value);
        return new PkidVector(newValues, missingPkids, missingPkidsEpoch, nextPkid);
    }

    @Override
    public PkidVector cons(final Object value) {
        final PersistentVector newValues = super.cons(value);
        return new PkidVector(newValues, missingPkids, missingPkidsEpoch, nextPkid + 1);
    }

    @Override
    public PkidVector pop() {
        final PersistentVector newValues = super.pop();
        long removedPkid = nextPkid - 1;

        while (missingPkids.contains(removedPkid)) {
            --removedPkid;
        }

        final long newMissingPkidsEpoch = missingPkidsEpoch + 1;
        final IntSet newMissingPkids = (IntSet) missingPkids.add(newMissingPkidsEpoch, removedPkid);
        return new PkidVector(newValues, newMissingPkids, newMissingPkidsEpoch, nextPkid);
    }

    @Override
    public PkidVector empty() {
        return _meta == null ? EMPTY : EMPTY.withMeta(_meta);
    }

    @Override
    public PkidVector withMeta(final IPersistentMap meta) {
        if (meta == _meta) {
            return this;
        }

        return new PkidVector(meta, cnt, shift, root, tail, missingPkids, missingPkidsEpoch, nextPkid);
    }

    /**
     * Clojure's final TransientVector cannot restore the extra pkid fields.
     */
    @Override
    public TransientVector asTransient() {
        throw new UnsupportedOperationException("PkidVector does not support generic transient conversion");
    }
}
