package com.dynamo.cr.protobind;


public class IndexElement extends PathElement {

    public int index;
    public IndexElement(int index) {
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