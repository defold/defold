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

/**
 * A persistent vector whose realized indexes are separate from its stable pkids.
 *
 * <p>This class lives in {@code clojure.lang} because {@link PersistentVector}
 * exposes only package-private constructors. Stable-pkid operations are provided
 * by the {@code util.pkid-vector} Clojure namespace. Normal vector operations use
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

    public final IPersistentSet missingPkids;
    public final long nextPkid;

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

    /**
     * Finalizes a transient values vector directly into a PkidVector.
     */
    public static PkidVector fromTransient(final ITransientVector transientValues,
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

    /**
     * Returns a transient view of the realized values for internal batch work.
     */
    public ITransientVector asTransientValues() {
        return super.asTransient();
    }

    /**
     * Returns a plain PersistentVector that shares this vector's backing data.
     */
    public PersistentVector asPersistentVector() {
        return new PersistentVector(_meta, cnt, shift, root, tail);
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

        while (missingPkids.contains(removedPkid)) {
            --removedPkid;
        }

        return new PkidVector(poppedValues,
                              (IPersistentSet) missingPkids.cons(removedPkid),
                              nextPkid);
    }

    @Override
    public PkidVector empty() {
        return new PkidVector((PersistentVector) super.empty(),
                              (IPersistentSet) missingPkids.empty(),
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
