package com.dynamo.cr.sceneed.gameobject;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.Messages;
import com.dynamo.cr.sceneed.core.NodeModel;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.cr.sceneed.core.Resource;

public class RefComponentNode extends ComponentNode {

    @Property(isResource=true)
    @Resource
    private String component;

    private NodePresenter presenter;
    private ComponentTypeNode type;

    public RefComponentNode(NodePresenter presenter, ComponentTypeNode type) {
        super();

        this.presenter = presenter;
        this.type = type;
    }

    @Override
    public void setModel(NodeModel model) {
        super.setModel(model);
        if (this.type != null) {
            this.type.setModel(model);
        }
    }

    public String getComponent() {
        return this.component;
    }

    public void setComponent(String component) {
        if (this.component != null ? !this.component.equals(component) : component != null) {
            this.component = component;
            reloadType();
            notifyChange();
        }
    }

    public ComponentTypeNode getType() {
        return this.type;
    }

    public IStatus validateComponent() {
        if (this.component != null && !this.component.equals("")) {
            if (this.type != null) {
                IStatus status = this.type.validate();
                if (!status.isOK()) {
                    return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.RefComponentNode_component_INVALID_REFERENCE);
                }
            } else {
                int index = this.component.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.RefComponentNode_component_UNKNOWN_TYPE, this.component);
                } else {
                    String ext = this.component.substring(index+1);
                    message = NLS.bind(Messages.RefComponentNode_component_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, message);
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus validate() {
        MultiStatus status = validateProperties(new String[] {"component"});
        status.merge(super.validate());
        return status;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.component);
    }

    @Override
    public boolean handleResourceChanged(IResourceDelta delta) {
        IResource resource = delta.getResource();
        IFile file = getModel().getFile(this.component);
        if (file.exists() && file.equals(resource)) {
            reloadType();
            notifyChange();
            return false;
        }
        return true;
    }

    private void reloadType() {
        this.type = null;
        try {
            this.type = (ComponentTypeNode)this.presenter.loadNode(this.component);
            if (this.type != null) {
                this.type.setModel(getModel());
            }
        } catch (Throwable e) {
            // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
        }
    }

}
