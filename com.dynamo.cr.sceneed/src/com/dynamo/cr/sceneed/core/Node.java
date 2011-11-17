package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
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
import com.dynamo.cr.sceneed.Messages;
import com.dynamo.cr.sceneed.SceneModel;

@Entity(commandFactory = SceneUndoableCommandFactory.class)
public class Node implements IAdaptable {

    private ISceneModel model;
    private List<Node> children = new ArrayList<Node>();
    private Node parent;

    private static Map<Class<? extends Node>, PropertyIntrospector<Node, ISceneModel>> introspectors =
            new HashMap<Class<? extends Node>, PropertyIntrospector<Node, ISceneModel>>();

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
            children.add(child);
            child.setParent(this);
            notifyChange();
        }
    }

    protected final void removeChild(Node child) {
        if (child != null && this.children.contains(child)) {
            children.remove(child);
            child.parent = null;
            notifyChange();
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
            notifyChange();
        }
    }

    protected final void notifyChange() {
        if (this.model != null) {
            this.model.notifyChange(this);
        }
    }

    public final Node getParent() {
        return this.parent;
    }

    private final void setParent(Node parent) {
        this.parent = parent;
        setModel(parent.getModel());
    }

    public Image getImage() {
        return null;
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
        IStatus ownStatus = doValidate();
        if (status != null) {
            ((MultiStatus)status).merge(ownStatus);
        } else {
            status = ownStatus;
        }
        return status;
    }

    protected final IStatus validateProperties(String[] properties) {
        MultiStatus status = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, SceneModel> model = (IPropertyModel<? extends Node, SceneModel>) getAdapter(IPropertyModel.class);
        for (String property : properties) {
            status.merge(model.getPropertyStatus(property));
        }
        return status;
    }

    protected IStatus doValidate() {
        return Status.OK_STATUS;
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        PropertyIntrospector<Node, ISceneModel> introspector = introspectors.get(this.getClass());

        if (introspector == null) {
            introspector = new PropertyIntrospector<Node, ISceneModel>(this.getClass(), Messages.class);
            introspectors.put(this.getClass(), introspector);
        }

        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<Node, ISceneModel>(this, getModel(), introspector);
        }
        return null;
    }

    /**
     * Override this to handle resource changes
     */
    public final void handleFileChanged(IFile file) {
        for (Node child : this.children) {
            child.handleReload(file);
        }
        handleReload(file);
    }

    public void handleReload(IFile file) {

    }

}
