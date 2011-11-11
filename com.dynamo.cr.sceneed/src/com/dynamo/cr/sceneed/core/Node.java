package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;

@Entity(commandFactory = NodeUndoableCommandFactory.class)
public class Node implements IAdaptable {

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

}
