package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.sceneed.Activator;

@Entity(commandFactory = NodeUndoableCommandFactory.class)
public class Node implements IAdaptable, IResourceDeltaVisitor {

    private NodeModel model;
    private List<Node> children;
    private Node parent;

    private static Map<Class<? extends Node>, PropertyIntrospector<Node, NodeModel>> introspectors =
            new HashMap<Class<? extends Node>, PropertyIntrospector<Node,NodeModel>>();

    public Node() {
        this.children = new ArrayList<Node>();
    }

    public NodeModel getModel() {
        return this.model;
    }

    public void setModel(NodeModel model) {
        this.model = model;
        for (Node child : this.children) {
            child.setModel(model);
        }
    }

    public List<Node> getChildren() {
        return Collections.unmodifiableList(this.children);
    }

    public boolean hasChildren() {
        return !this.children.isEmpty();
    }

    protected void addChild(Node child) {
        if (!this.children.contains(child)) {
            children.add(child);
            child.setParent(this);
            notifyChange();
        }
    }

    protected void removeChild(Node child) {
        if (this.children.contains(child)) {
            children.remove(child);
            child.parent = null;
            notifyChange();
        }
    }

    protected void sortChildren(Comparator<? super Node> comparator) {
        List<Node> sortedChildren = new ArrayList<Node>(this.children);
        Collections.sort(sortedChildren, comparator);
        if (!sortedChildren.equals(this.children)) {
            this.children = sortedChildren;
            notifyChange();
        }
    }

    protected void notifyChange() {
        if (this.model != null) {
            this.model.notifyChange(this);
        }
    }

    public Node getParent() {
        return this.parent;
    }

    private void setParent(Node parent) {
        this.parent = parent;
        setModel(parent.getModel());
    }

    public Image getImage() {
        return null;
    }

    public IStatus validate() {
        if (!this.children.isEmpty()) {
            MultiStatus status = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            for (Node child : this.children) {
                status.merge(child.validate());
            }
            return status;
        }
        return Status.OK_STATUS;
    }

    protected MultiStatus validateProperties(String[] properties) {
        MultiStatus status = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, NodeModel> model = (IPropertyModel<? extends Node, NodeModel>) getAdapter(IPropertyModel.class);
        for (String property : properties) {
            status.merge(model.getPropertyStatus(property));
        }
        return status;
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        PropertyIntrospector<Node, NodeModel> introspector = introspectors.get(this.getClass());

        if (introspector == null) {
            introspector = new PropertyIntrospector<Node, NodeModel>(this.getClass(), Messages.class);
            introspectors.put(this.getClass(), introspector);
        }

        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<Node, NodeModel>(this, getModel(), introspector);
        }
        return null;
    }

    @Override
    public final boolean visit(IResourceDelta delta) throws CoreException {
        for (Node child : this.children) {
            delta.accept(child);
        }
        return handleResourceChanged(delta);
    }

    /**
     * Override this to handle resource changes
     * @param delta
     */
    public boolean handleResourceChanged(IResourceDelta delta) {
        return false;
    }

}
