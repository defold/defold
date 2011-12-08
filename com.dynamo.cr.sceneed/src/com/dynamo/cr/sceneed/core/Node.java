package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.PreDestroy;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.sceneed.Activator;

@Entity(commandFactory = SceneUndoableCommandFactory.class)
public abstract class Node implements IAdaptable {

    private ISceneModel model;
    private List<Node> children = new ArrayList<Node>();
    private Node parent;

    @Property
    protected Vector4d translation = new Vector4d(0, 0, 0, 0);

    @Property
    protected Quat4d rotation = new Quat4d(0, 0, 0, 1);

    // Used to preserve order when adding/remvoing child nodes
    private int childIndex = -1;

    private static Map<Class<? extends Node>, PropertyIntrospector<Node, ISceneModel>> introspectors =
            new HashMap<Class<? extends Node>, PropertyIntrospector<Node, ISceneModel>>();

    @PreDestroy
    public void dispose() {
        List<Node> children = new ArrayList<Node>(this.children);
        int n = children.size();
        for (int i = n-1; i >= 0; --i) {
            removeChild(children.get(i));
        }
    }

    public Node() {
    }

    public Node(Vector4d translation, Quat4d rotation) {
        this.translation.set(translation);
        this.rotation.set(rotation);
    }

    public void setTranslation(Vector4d translation) {
        this.translation.set(translation);
    }

    public Vector4d getTranslation() {
        return new Vector4d(translation);
    }

    public void setRotation(Quat4d rotation) {
        this.rotation.set(rotation);
    }

    public Quat4d getRotation() {
        return new Quat4d(rotation);
    }

    public void getLocalTransform(Matrix4d transform)
    {
        transform.setIdentity();
        transform.setColumn(3, translation);
        transform.m33 = 1;
        transform.setRotation(rotation);
    }

    public void getWorldTransform(Matrix4d transform) {
        Matrix4d tmp = new Matrix4d();
        transform.setIdentity();
        Node n = this;
        while (n != null)
        {
            n.getLocalTransform(tmp);
            transform.mul(tmp, transform);
            n = n.getParent();
        }
    }

    public final ISceneModel getModel() {
        return this.model;
    }

    public void setModel(ISceneModel model) {
        this.model = model;
        for (Node child : this.children) {
            child.setModel(model);
        }
    }

    public final List<Node> getChildren() {
        return Collections.unmodifiableList(this.children);
    }

    public final boolean hasChildren() {
        return !this.children.isEmpty();
    }

    protected final void addChild(Node child) {
        if (child != null && !this.children.contains(child)) {
            if (child.childIndex >= 0 && child.childIndex < children.size()) {
                children.add(child.childIndex, child);
            } else {
                children.add(child);
                child.childIndex = children.size() - 1;
            }
            child.setParent(this);
        }
    }

    protected final void removeChild(Node child) {
        if (child != null && this.children.contains(child)) {
            child.childIndex = this.children.indexOf(child);
            children.remove(child);
            child.setParent(null);
        }
    }

    protected final void clearChildren() {
        this.children.clear();
    }

    protected final void sortChildren(Comparator<? super Node> comparator) {
        List<Node> sortedChildren = new ArrayList<Node>(this.children);
        Collections.sort(sortedChildren, comparator);
        if (!sortedChildren.equals(this.children)) {
            this.children = sortedChildren;
        }

        int i = 0;
        for (Node child : children) {
            child.childIndex = i++;
        }
    }

    public final Node getParent() {
        return this.parent;
    }

    private final void setParent(Node parent) {
        this.parent = parent;
        if (parent != null) {
            setModel(parent.getModel());
        } else {
            setModel(null);
        }
    }

    public Image getIcon() {
        if (this.model != null) {
            return this.model.getIcon(getClass());
        } else {
            return null;
        }
    }

    public final IStatus validate() {
        IStatus status = null;
        if (!this.children.isEmpty()) {
            MultiStatus multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            for (Node child : this.children) {
                multiStatus.merge(child.validate());
            }
            status = multiStatus;
        }
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> model = (IPropertyModel<? extends Node, ISceneModel>) getAdapter(IPropertyModel.class);
        IStatus ownStatus = model.getStatus();
        if (status != null) {
            ((MultiStatus)status).merge(ownStatus);
        } else {
            status = ownStatus;
        }

        IStatus nodeStatus = validateNode();
        if (status.isOK()) {
            status = nodeStatus;
        } else {
            if (!(status instanceof MultiStatus)) {
                MultiStatus multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
                multiStatus.merge(status);
                status = multiStatus;
            }
            ((MultiStatus)status).merge(nodeStatus);
        }

        return status;
    }

    protected IStatus validateNode() {
        return Status.OK_STATUS;
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        PropertyIntrospector<Node, ISceneModel> introspector = introspectors.get(this.getClass());

        if (introspector == null) {
            introspector = new PropertyIntrospector<Node, ISceneModel>(this.getClass());
            introspectors.put(this.getClass(), introspector);
        }

        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<Node, ISceneModel>(this, getModel(), introspector);
        }
        return null;
    }

    /**
     * Override to handle reload
     * @param file
     * @return True if anything was reloaded
     */
    public boolean handleReload(IFile file) {
        return false;
    }

    @Override
    public String toString() {
        if (this.model != null) {
            String typeName = this.model.getTypeName(getClass());
            if (typeName != null) {
                return typeName;
            }
        }
        return super.toString();
    }
}
