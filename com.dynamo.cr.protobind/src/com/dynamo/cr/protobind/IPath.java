package com.dynamo.cr.protobind;


public interface IPath {

    /**
     * Get path element at index
     * @param index index
     * @return path element at index
     */
    public abstract PathElement element(int index);

    /**
     * Get path element count
     * @return path element count
     */
    public abstract int elementCount();

    /**
     * Get name of the last element of the path. For repeated fields the index as a string is returned.
     * @return last element name
     */
    public abstract String getName();

    /**
     * Get path to parent
     * @return path to parent. null if at root
     */
    public abstract IPath getParent();

    /**
     * Get last path element.
     * @return last element
     */
    public abstract PathElement lastElement();

}
