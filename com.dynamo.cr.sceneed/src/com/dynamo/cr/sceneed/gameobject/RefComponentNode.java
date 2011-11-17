package com.dynamo.cr.sceneed.gameobject;

import java.util.List;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.Messages;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.Resource;

public class RefComponentNode extends ComponentNode {

    @Property(isResource=true)
    @Resource
    private String component;
    private ComponentTypeNode type;

    public RefComponentNode(ComponentTypeNode type) {
        super();
        this.type = type;
        this.type.setModel(this.getModel());
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
    public IStatus doValidate() {
        IStatus status = validateProperties(new String[] {"component"});
        MultiStatus multiStatus= null;
        if (status.isMultiStatus()) {
            multiStatus = (MultiStatus)status;
        } else {
            multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            multiStatus.merge(status);
        }
        multiStatus.merge(super.doValidate());
        return multiStatus;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.component);
    }

    @Override
    public void handleReload(IFile file) {
        IFile componentFile = getModel().getFile(this.component);
        if (componentFile.exists() && componentFile.equals(file)) {
            reloadType();
        }
    }

    private void reloadType() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                this.type = (ComponentTypeNode)getModel().loadNode(this.component);
                this.type.setModel(this.getModel());
                notifyChange();
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
        }
    }

}
