package com.dynamo.cr.tileeditor.scene;

import java.awt.Color;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
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

    private boolean nameRegistered;

    private static CollisionGroupIndexPool cache = new CollisionGroupIndexPool(Activator.MAX_COLLISION_GROUP_COUNT);

    public CollisionGroupNode() {
        this.name = "";
        this.nameRegistered = false;
    }

    public CollisionGroupNode(String name) {
        this.name = name;
    }

    public String getName() {
        return this.name;
    }

    public void setName(String name) {
        if (!this.name.equals(name)) {
            unregisterName();
            this.name = name;
            registerName();

            notifyChange();
        }
    }

    public IStatus validateName() {
        if (!this.name.isEmpty()) {
            registerName();
            if (!this.nameRegistered) {
                return new Status(IStatus.WARNING, Activator.PLUGIN_ID, NLS.bind(Messages.CollisionGroupNode_name_OVERFLOW, Activator.MAX_COLLISION_GROUP_COUNT));
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public void setModel(ISceneModel model) {
        if (getModel() != model) {
            unregisterName();
            super.setModel(model);
            registerName();
        }
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getCollisionGroupImage(cache.get(this.name));
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

    public static Color getCollisionGroupColor(String name) {
        return Activator.getDefault().getCollisionGroupColor(cache.get(name));
    }

    public static void clearCollisionGroups() {
        cache.clear();
    }

    private void registerName() {
        if (!this.nameRegistered && !this.name.isEmpty() && getModel() != null) {
            if (cache.add(this.name)) {
                this.nameRegistered = true;
            }
        }
    }

    private void unregisterName() {
        if (this.nameRegistered) {
            cache.remove(this.name);
            this.nameRegistered = false;
        }
    }
}
