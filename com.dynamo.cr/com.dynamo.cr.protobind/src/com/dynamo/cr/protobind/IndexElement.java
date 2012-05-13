package com.dynamo.cr.protobind;

import com.google.protobuf.Descriptors.FieldDescriptor;


public class IndexElement extends PathElement {

    public int index;

    public IndexElement(int index, FieldDescriptor fieldDescriptor) {
        this.fieldDescriptor = fieldDescriptor;
        this.index = index;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof IndexElement) {
            IndexElement other = (IndexElement) obj;
            return this.index == other.index;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return index;
    }

    @Override
    public String toString() {
        return Integer.toString(index);
    }

    @Override
    public boolean isField() {
        return false;
    }

    @Override
    public boolean isIndex() {
        return true;
    }
}