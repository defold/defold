package com.dynamo.cr.tileeditor.scene;

import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;

public class CollisionGroupNode extends Node implements
Comparable<CollisionGroupNode> {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    private String name;

    public CollisionGroupNode() {
        this.name = "";
    }

    public CollisionGroupNode(String name) {
        this.name = name;
    }

    public String getName() {
        return this.name;
    }

    public void setName(String name) {
        if (this.name != null ? !this.name.equals(name) : name != null) {
            this.name = name;
            notifyChange();
        }
    }

    public TileSetNode getTileSetNode() {
        return (TileSetNode) getParent();
    }

    @Override
    public String toString() {
        return this.name;
    }

    @Override
    public int compareTo(CollisionGroupNode o) {
        return this.name.compareTo(o.toString());
    }

}
