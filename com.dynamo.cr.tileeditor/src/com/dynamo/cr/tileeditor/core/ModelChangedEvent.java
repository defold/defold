package com.dynamo.cr.tileeditor.core;

public class ModelChangedEvent {

    public static final int CHANGE_FLAG_PROPERTIES = (1 << 0);

    public int changes;

    public ModelChangedEvent(int changes) {
        this.changes = changes;
    }
}
