package com.dynamo.cr.protobind;

public abstract class PathElement {

    /**
     * Is the element a field reference
     * @return true if field reference
     */
    public abstract boolean isField();

    /**
     * Is the element a index reference, ie repeated index
     * @return true if index reference
     */
    public abstract boolean isIndex();
}