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

import java.util.Arrays;
import java.util.Iterator;

/**
 * A persistent vector whose realized indexes are separate from its stable pkids.
 *
 * <p>This class lives in {@code clojure.lang} because {@link PersistentVector}
 * exposes only package-private constructors. Normal vector operations use
 * realized indexes, and vector equality and hashing consider only realized
 * values.</p>
 *
 * <p>Clojure's transient vector implementation is final and cannot retain the
 * additional fields, so generic transient conversion is rejected. Consequently,
 * this class cannot be used as the target of transient-optimized operations such
 * as {@code into}. This class also does not define a Java serialization format
 * for its pkid state.</p>
 */
public final class PkidVector extends PersistentVector {
    private static final long serialVersionUID = 1L;

    private static final Var INT_MAP_RANGE = RT.var("clojure.data.int-map", "range");
    private static final long[] EMPTY_PKIDS = new long[0];

    public final IPersistentSet missingPkids;
    public final long nextPkid;

    /**
     * Creates a pkid vector backed by an ordered, editable
     * {@code clojure.data.int-map} set of missing pkids.
     */
    public PkidVector(final PersistentVector values,
                      final IPersistentSet missingPkids,
                      final long nextPkid) {
        this(values._meta,
             values.cnt,
             values.shift,
             values.root,
             values.tail,
             missingPkids,
             nextPkid);
    }

    private PkidVector(final IPersistentMap meta,
                       final int count,
                       final int shift,
                       final Node root,
                       final Object[] tail,
                       final IPersistentSet missingPkids,
                       final long nextPkid) {
        super(meta, count, shift, root, tail);
        this.missingPkids = missingPkids;
        this.nextPkid = nextPkid;
    }

    private static PkidVector fromTransient(final ITransientVector transientValues,
                                            final IPersistentMap meta,
                                            final IPersistentSet missingPkids,
                                            final long nextPkid) {
        final TransientVector transientVector = (TransientVector) transientValues;
        transientVector.ensureEditable();
        transientVector.root.edit.set(null);

        final int count = transientVector.cnt;
        final int tailOffset = count < 32 ? 0 : ((count - 1) >>> 5) << 5;
        final Object[] tail = new Object[count - tailOffset];
        System.arraycopy(transientVector.tail, 0, tail, 0, tail.length);

        return new PkidVector(meta,
                              count,
                              transientVector.shift,
                              transientVector.root,
                              tail,
                              missingPkids,
                              nextPkid);
    }

    private static ITransientSet asTransientSet(final IPersistentSet set) {
        return (ITransientSet) ((IEditableCollection) set).asTransient();
    }

    private IPersistentSet emptyMissingPkids() {
        if (missingPkids.count() == 0) {
            return missingPkids;
        }

        return (IPersistentSet) missingPkids.empty();
    }

    private IPersistentSet asPersistentMissingPkids(final ITransientSet set) {
        return (IPersistentSet) set.persistent();
    }

    private static ITransientVector conj(final ITransientVector vector, final Object value) {
        return (ITransientVector) vector.conj(value);
    }

    private static ITransientSet conj(final ITransientSet set, final long value) {
        return (ITransientSet) set.conj(Long.valueOf(value));
    }

    private static ITransientSet disjoin(final ITransientSet set, final long value) {
        return set.disjoin(Long.valueOf(value));
    }

    private static long checkedPkid(final Object value) {
        if (!(value instanceof Long
              || value instanceof Integer
              || value instanceof Short
              || value instanceof Byte)) {
            throw new AssertionError("Assert failed: (nat-int? pkid)");
        }

        final long pkid = ((Number) value).longValue();
        if (pkid < 0) {
            throw new AssertionError("Assert failed: (nat-int? pkid)");
        }

        return pkid;
    }

    private static int initialPkidCapacity(final Object pkids) {
        if (pkids instanceof Counted) {
            return Math.min(((Counted) pkids).count(), 8);
        }

        return pkids == null ? 0 : 8;
    }

    private static ITransientVector conjValues(final ITransientVector result,
                                               final PersistentVector values,
                                               final int startIndex,
                                               final int endIndex) {
        ITransientVector newResult = result;
        int index = startIndex;

        while (index < endIndex) {
            final Object[] array = values.arrayFor(index);
            int offset = index & 0x01f;
            final int limit = Math.min(array.length, offset + endIndex - index);

            while (offset < limit) {
                newResult = conj(newResult, array[offset]);
                ++offset;
                ++index;
            }
        }

        return newResult;
    }

    private static ITransientSet conjPkidRange(ITransientSet result,
                                               final long firstPkid,
                                               final long endPkid) {
        for (long pkid = firstPkid; pkid < endPkid; ++pkid) {
            result = conj(result, pkid);
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

    private static long missingPkidCountInRange(final IPersistentSet missingPkids,
                                                final long startPkid,
                                                final long endPkid) {
        if (startPkid == endPkid) {
            return 0;
        }

        final IFn.OLLO rangeFunction = (IFn.OLLO) INT_MAP_RANGE.getRawRoot();
        final IPersistentCollection range =
                (IPersistentCollection) rangeFunction.invokePrim(missingPkids,
                                                                  startPkid,
                                                                  endPkid - 1);
        return range.count();
    }

    private static long pkidAtIndex(final IPersistentSet missingPkids,
                                    final long nextPkid,
                                    final long index) {
        long low = index;
        long high = nextPkid - 1;

        while (low != high) {
            final long mid = low + (high - low) / 2;
            final long liveCountThroughMid =
                    mid + 1 - missingPkidCountInRange(missingPkids, 0, mid + 1);

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

    private static final class MissingPkidCursor {
        private final Iterator<?> iterator;
        private boolean hasValue;
        private long value;
        private long countBefore;

        MissingPkidCursor(final IPersistentSet missingPkids) {
            iterator = RT.iter(missingPkids);
            advance();
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
            hasValue = iterator.hasNext();
            if (hasValue) {
                value = ((Number) iterator.next()).longValue();
            }
        }
    }

    private void addAssocPkid(final PkidBatch batch,
                              final Object value,
                              final boolean hasMissingPkids) {
        final long pkid = checkedPkid(value);
        batch.add(pkid);

        if (!batch.restoresMissing
            && hasMissingPkids
            && pkid < nextPkid
            && missingPkids.contains(Long.valueOf(pkid))) {
            batch.restoresMissing = true;
        }
    }

    private void addDissocPkid(final PkidBatch batch,
                               final Object value,
                               final boolean hasMissingPkids) {
        final long pkid = checkedPkid(value);
        if (pkid < nextPkid
            && (!hasMissingPkids || !missingPkids.contains(Long.valueOf(pkid)))) {
            batch.add(pkid);
        }
    }

    private PkidBatch normalizeAssocPkids(final Object pkids) {
        final PkidBatch batch = new PkidBatch(initialPkidCapacity(pkids));
        final boolean hasMissingPkids = missingPkids.count() != 0;

        if (pkids instanceof IReduceInit
            && !(pkids instanceof Iterable)
            && !(pkids instanceof Seqable)) {
            ((IReduceInit) pkids).reduce(new AFn() {
                @Override
                public Object invoke(final Object result, final Object value) {
                    addAssocPkid(batch, value, hasMissingPkids);
                    return result;
                }
            }, null);
        } else {
            final Iterator<?> iterator = RT.iter(pkids);
            while (iterator.hasNext()) {
                addAssocPkid(batch, iterator.next(), hasMissingPkids);
            }
        }

        batch.finish();
        return batch;
    }

    private PkidBatch normalizeDissocPkids(final Object pkids) {
        final PkidBatch batch = new PkidBatch(0);
        final boolean hasMissingPkids = missingPkids.count() != 0;

        if (pkids instanceof IReduceInit
            && !(pkids instanceof Iterable)
            && !(pkids instanceof Seqable)) {
            ((IReduceInit) pkids).reduce(new AFn() {
                @Override
                public Object invoke(final Object result, final Object value) {
                    addDissocPkid(batch, value, hasMissingPkids);
                    return result;
                }
            }, null);
        } else {
            final Iterator<?> iterator = RT.iter(pkids);
            while (iterator.hasNext()) {
                addDissocPkid(batch, iterator.next(), hasMissingPkids);
            }
        }

        batch.finish();
        return batch;
    }

    private PkidVector assocPkidsWithoutRestoration(final PkidBatch batch,
                                                    final Object value) {
        final MissingPkidCursor missingCursor =
                missingPkids.count() == 0 ? null : new MissingPkidCursor(missingPkids);
        ITransientVector newValues = super.asTransient();
        ITransientSet newMissingPkids = null;
        long newNextPkid = nextPkid;

        for (int batchIndex = 0; batchIndex < batch.count; ++batchIndex) {
            final long pkid = batch.pkids[batchIndex];

            if (pkid < nextPkid) {
                final long missingCountBefore =
                        missingCursor == null ? 0 : missingCursor.countBefore(pkid);
                final int index = Math.toIntExact(pkid - missingCountBefore);
                newValues = newValues.assocN(index, value);
            } else {
                if (newMissingPkids == null && newNextPkid < pkid) {
                    newMissingPkids = asTransientSet(missingPkids);
                }

                if (newNextPkid < pkid) {
                    newMissingPkids = conjPkidRange(newMissingPkids, newNextPkid, pkid);
                }

                newNextPkid = pkid + 1;
                newValues = conj(newValues, value);
            }
        }

        final IPersistentSet resultMissingPkids =
                newMissingPkids == null
                ? missingPkids
                : asPersistentMissingPkids(newMissingPkids);

        return fromTransient(newValues, _meta, resultMissingPkids, newNextPkid);
    }

    private PkidVector assocPkidsWithRestoration(final PkidBatch batch,
                                                 final Object value) {
        final MissingPkidCursor missingCursor = new MissingPkidCursor(missingPkids);
        ITransientVector newValues = PersistentVector.EMPTY.asTransient();
        ITransientSet newMissingPkids = asTransientSet(missingPkids);
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
                    newMissingPkids = disjoin(newMissingPkids, pkid);
                    sourceIndex = index;
                } else {
                    sourceIndex = index + 1;
                }

                newValues = conj(newValues, value);
            } else {
                newValues = conjValues(newValues, this, sourceIndex, cnt);
                sourceIndex = cnt;
                newMissingPkids = conjPkidRange(newMissingPkids, newNextPkid, pkid);
                newNextPkid = pkid + 1;
                newValues = conj(newValues, value);
            }
        }

        newValues = conjValues(newValues, this, sourceIndex, cnt);
        return fromTransient(newValues,
                             _meta,
                             asPersistentMissingPkids(newMissingPkids),
                             newNextPkid);
    }

    public PkidVector append(final Object value) {
        return cons(value);
    }

    public PkidVector assocPkids(final Object pkids, final Object value) {
        if (pkids == null || (pkids instanceof Counted && ((Counted) pkids).count() == 0)) {
            return this;
        }

        final PkidBatch batch = normalizeAssocPkids(pkids);
        if (batch.count == 0) {
            return this;
        }

        if (batch.restoresMissing) {
            return assocPkidsWithRestoration(batch, value);
        }

        return assocPkidsWithoutRestoration(batch, value);
    }

    public PkidVector dissocPkids(final Object pkids) {
        if (pkids == null || (pkids instanceof Counted && ((Counted) pkids).count() == 0)) {
            return this;
        }

        final PkidBatch batch = normalizeDissocPkids(pkids);
        if (batch.count == 0) {
            return this;
        }

        final MissingPkidCursor missingCursor =
                missingPkids.count() == 0 ? null : new MissingPkidCursor(missingPkids);
        ITransientVector newValues = PersistentVector.EMPTY.asTransient();
        ITransientSet newMissingPkids = asTransientSet(missingPkids);
        int sourceIndex = 0;

        for (int batchIndex = 0; batchIndex < batch.count; ++batchIndex) {
            final long pkid = batch.pkids[batchIndex];
            final long missingCountBefore =
                    missingCursor == null ? 0 : missingCursor.countBefore(pkid);
            final int index = Math.toIntExact(pkid - missingCountBefore);
            newValues = conjValues(newValues, this, sourceIndex, index);
            sourceIndex = index + 1;
            newMissingPkids = conj(newMissingPkids, pkid);
        }

        newValues = conjValues(newValues, this, sourceIndex, cnt);
        return fromTransient(newValues,
                             _meta,
                             asPersistentMissingPkids(newMissingPkids),
                             nextPkid);
    }

    public IPersistentSet findPkids(final Object value) {
        final int missingCount = missingPkids.count();
        final IPersistentSet emptyResult = emptyMissingPkids();

        if (missingCount == 0) {
            ITransientSet result = null;
            int index = 0;

            while (index < cnt) {
                final Object[] array = arrayFor(index);
                for (int offset = 0; offset < array.length; ++offset, ++index) {
                    if (Util.equiv(value, array[offset])) {
                        if (result == null) {
                            result = asTransientSet(emptyResult);
                        }

                        result = conj(result, index);
                    }
                }
            }

            return result == null ? emptyResult : asPersistentMissingPkids(result);
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
                        final int newCapacity =
                                matchingIndexCount < 8 ? 8 : Math.multiplyExact(matchingIndexCount, 2);
                        matchingIndexes = Arrays.copyOf(matchingIndexes, newCapacity);
                    }

                    matchingIndexes[matchingIndexCount] = index;
                    ++matchingIndexCount;
                }
            }
        }

        if (matchingIndexCount == 0) {
            return emptyResult;
        }

        ITransientSet result = asTransientSet(emptyResult);
        if (missingCount <= (long) matchingIndexCount * binarySearchHeight(nextPkid)) {
            int matchingIndexPosition = 0;
            long missingPkidCount = 0;
            final Iterator<?> missingIterator = RT.iter(missingPkids);

            while (missingIterator.hasNext() && matchingIndexPosition < matchingIndexCount) {
                final long missingPkid = ((Number) missingIterator.next()).longValue();

                while (matchingIndexPosition < matchingIndexCount) {
                    final long pkid = matchingIndexes[matchingIndexPosition] + missingPkidCount;
                    if (pkid >= missingPkid) {
                        break;
                    }

                    result = conj(result, pkid);
                    ++matchingIndexPosition;
                }

                ++missingPkidCount;
            }

            while (matchingIndexPosition < matchingIndexCount) {
                result = conj(result,
                              matchingIndexes[matchingIndexPosition] + missingPkidCount);
                ++matchingIndexPosition;
            }
        } else {
            for (int matchingIndexPosition = 0;
                 matchingIndexPosition < matchingIndexCount;
                 ++matchingIndexPosition) {
                result = conj(result,
                              pkidAtIndex(missingPkids,
                                          nextPkid,
                                          matchingIndexes[matchingIndexPosition]));
            }
        }

        return asPersistentMissingPkids(result);
    }

    @Override
    public PkidVector assocN(final int index, final Object value) {
        if (index == cnt) {
            return cons(value);
        }

        return new PkidVector(super.assocN(index, value), missingPkids, nextPkid);
    }

    @Override
    public PkidVector cons(final Object value) {
        return new PkidVector(super.cons(value), missingPkids, nextPkid + 1);
    }

    @Override
    public PkidVector pop() {
        final PersistentVector poppedValues = super.pop();
        long removedPkid = nextPkid - 1;

        while (missingPkids.contains(Long.valueOf(removedPkid))) {
            --removedPkid;
        }

        return new PkidVector(poppedValues,
                              (IPersistentSet) missingPkids.cons(Long.valueOf(removedPkid)),
                              nextPkid);
    }

    @Override
    public PkidVector empty() {
        return new PkidVector((PersistentVector) super.empty(),
                              emptyMissingPkids(),
                              0);
    }

    @Override
    public PkidVector withMeta(final IPersistentMap meta) {
        if (meta == _meta) {
            return this;
        }

        return new PkidVector(meta,
                              cnt,
                              shift,
                              root,
                              tail,
                              missingPkids,
                              nextPkid);
    }

    /**
     * Clojure's final TransientVector cannot restore the extra pkid fields.
     */
    @Override
    public TransientVector asTransient() {
        throw new UnsupportedOperationException(
                "PkidVector does not support generic transient conversion");
    }
}
