package com.dynamo.cr.go.core;

import com.dynamo.cr.sceneed.core.Node;

public abstract class ComponentTypeNode extends Node {

    private String path;

    public String getPath() {
        return this.path;
    }

    public void setPath(String path) {
        this.path = path;
    }

    @Override
    public String toString() {
        if (this.path != null) {
            return this.path;
        } else {
            return super.toString();
        }
    }
}
