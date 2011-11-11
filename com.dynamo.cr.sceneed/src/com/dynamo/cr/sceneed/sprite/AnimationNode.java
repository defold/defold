package com.dynamo.cr.sceneed.sprite;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;

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
