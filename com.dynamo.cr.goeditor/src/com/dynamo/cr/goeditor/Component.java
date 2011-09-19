package com.dynamo.cr.goeditor;

public abstract class Component {
    private int index = -1;
    private String id;

    public int getIndex() {
        return index;
    }

    public void setIndex(int index) {
        this.index = index;
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public abstract String getFileExtension();

}
