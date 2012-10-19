package com.dynamo.cr.editor.core.operations;

import org.eclipse.core.commands.operations.IUndoableOperation;


/**
 * Interface to impmenent for mergeable {@link IUndoableOperation}
 * @author chmu
 *
 */
public interface IMergeableOperation {

    /**
     * Operation merge type
     *
     */
    public enum Type {
        /**
         * Open operation
         */
        OPEN,

        /**
         * Intermediate operation
         */
        INTERMEDIATE,

        /**
         * Close operation. If current operation is mergeable
         * with a close operation current operation will be set to
         * CLOSE
         */
        CLOSE,
    }

    /**
     * Set operation type
     * <p>
     * <b>NOTE:</b> by convention type is OPEN by default
     * @param type {@link Type} to set
     */
    public void setType(Type type);

    /**
     * Get operaiton type
     * <p>
     * <b>NOTE:</b> by convention type is OPEN by default
     * @return {@link Type}
     */
    public Type getType();

    /**
     * Check if the operation is mergeable with supplied operaiotn
     * @param operation operation to test with
     * @return true of mergeable
     */
    public boolean canMerge(IMergeableOperation operation);

    /**
     * Merge operation in-place with his
     * @param operation operation to merge with
     */
    public void mergeWith(IMergeableOperation operation);
}
