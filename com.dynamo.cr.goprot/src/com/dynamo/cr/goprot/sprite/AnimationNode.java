package com.dynamo.cr.goprot.sprite;

import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.properties.Property;

public class AnimationNode extends Node {

    @Property
    private String id;

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        if (this.id != null ? !this.id.equals(id) : id != null) {
            this.id = id;
            notifyChange();
        }
    }

    @Override
    public String toString() {
        return this.id;
    }

}
