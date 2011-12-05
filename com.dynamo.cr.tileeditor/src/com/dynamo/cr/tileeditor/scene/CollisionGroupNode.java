package com.dynamo.cr.tileeditor.scene;

import javax.annotation.PreDestroy;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;

public class CollisionGroupNode extends Node implements
Comparable<CollisionGroupNode> {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    private String name;

    private Image icon;

    public CollisionGroupNode() {
        this.name = "";
        this.icon = null;
    }

    public CollisionGroupNode(String name) {
        this.name = name;
    }

    @Override
    @PreDestroy
    public void dispose() {
        clearIcon();
    }

    public String getName() {
        return this.name;
    }

    public void setName(String name) {
        if (this.name == null ? name != null : !this.name.equals(name)) {
            clearIcon();
            this.name = name;

            notifyChange();
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        if (model == null) {
            clearIcon();
        }
        super.setModel(model);
    }

    @Override
    public Image getIcon() {
        if (this.icon == null && !this.name.isEmpty()) {
            Activator activator = Activator.getDefault();
            activator.addCollisionGroup(this.name);
            this.icon = activator.getCollisionGroupIcon(this.name);
        }
        return this.icon;
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

    private void clearIcon() {
        if (this.icon != null) {
            this.icon = null;
            Activator.getDefault().removeCollisionGroup(this.name);
        }
    }
}
