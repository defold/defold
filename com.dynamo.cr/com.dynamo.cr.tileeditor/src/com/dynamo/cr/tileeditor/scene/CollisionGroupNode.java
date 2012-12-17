package com.dynamo.cr.tileeditor.scene;

import java.awt.Color;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.cr.tileeditor.Activator;

@SuppressWarnings("serial")
public class CollisionGroupNode extends Node implements
Comparable<CollisionGroupNode>, Identifiable {

    @Property
    @Unique(scope = CollisionGroupNode.class)
    @NotEmpty(severity = IStatus.ERROR)
    private String id;

    private boolean idRegistered;

    private static CollisionGroupIndexPool cache = new CollisionGroupIndexPool(Activator.MAX_COLLISION_GROUP_COUNT);

    public CollisionGroupNode() {
        this.id = "default";
        this.idRegistered = false;
    }

    public CollisionGroupNode(String id) {
        this.id = id;
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        if (!this.id.equals(id)) {
            unregisterId();
            this.id = id;
            registerId();
            TileSetNode tileSet = getTileSetNode();
            if (tileSet != null) {
                tileSet.sortChildren();
                tileSet.updateConvexHulls();
            }
        }
    }

    public IStatus validateId() {
        if (!this.id.isEmpty()) {
            registerId();
            if (!this.idRegistered) {
                return new Status(IStatus.WARNING, Activator.PLUGIN_ID, NLS.bind(Messages.CollisionGroupNode_id_OVERFLOW, Activator.MAX_COLLISION_GROUP_COUNT));
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public void setModel(ISceneModel model) {
        if (getModel() != model) {
            unregisterId();
            super.setModel(model);
            registerId();
        }
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getCollisionGroupImage(cache.get(this.id));
    }

    public TileSetNode getTileSetNode() {
        return (TileSetNode)getParent();
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public int compareTo(CollisionGroupNode o) {
        return this.id.compareTo(o.toString());
    }

    public static Color getCollisionGroupColor(String id) {
        return Activator.getDefault().getCollisionGroupColor(cache.get(id));
    }

    public static void clearCollisionGroups() {
        cache.clear();
    }

    private void registerId() {
        if (!this.idRegistered && !this.id.isEmpty() && getModel() != null) {
            if (cache.add(this.id)) {
                this.idRegistered = true;
            }
        }
    }

    private void unregisterId() {
        if (this.idRegistered) {
            cache.remove(this.id);
            this.idRegistered = false;
        }
    }
}
