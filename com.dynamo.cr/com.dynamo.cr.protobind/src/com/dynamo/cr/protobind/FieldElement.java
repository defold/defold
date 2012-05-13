package com.dynamo.cr.protobind;

import com.google.protobuf.Descriptors.FieldDescriptor;

public class FieldElement extends PathElement {

    public FieldElement(FieldDescriptor fieldDescriptor) {
        this.fieldDescriptor = fieldDescriptor;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof FieldElement) {
            FieldElement other = (FieldElement) obj;
            return this.fieldDescriptor.equals(other.fieldDescriptor);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return fieldDescriptor.hashCode();
    }

    @Override
    public String toString() {
        return fieldDescriptor.getName();
    }

    @Override
    public boolean isField() {
        return true;
    }

    @Override
    public boolean isIndex() {
        return false;
    }
}
