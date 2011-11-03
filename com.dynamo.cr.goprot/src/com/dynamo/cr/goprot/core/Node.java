package com.dynamo.cr.goprot.core;

import java.beans.PropertyChangeEvent;

public class Node {

    private NodeModel model;
    private boolean selected;

    public boolean isSelected() {
        return selected;
    }

    public void setSelected(boolean selected) {
        if (this.selected != selected) {
            notifyChange();
            this.selected = selected;
        }
    }

    public NodeModel getModel() {
        return this.model;
    }

    public void setModel(NodeModel model) {
        this.model = model;
    }

    protected void notifyChange() {
        if (this.model != null) {
            this.model.firePropertyChangeEvent(new PropertyChangeEvent(this, null, null, null));
        }
    }
}
