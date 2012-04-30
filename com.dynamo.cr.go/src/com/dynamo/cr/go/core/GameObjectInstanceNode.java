package com.dynamo.cr.go.core;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.go.Constants;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;

@SuppressWarnings("serial")
public class GameObjectInstanceNode extends InstanceNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"go"})
    @Resource
    @NotEmpty
    private String gameObject = "";

    private transient GameObjectNode gameObjectNode;

    public GameObjectInstanceNode() {
        super();
    }

    public GameObjectInstanceNode(GameObjectNode gameObject) {
        super();
        this.gameObjectNode = gameObject;
        if (this.gameObjectNode != null) {
            this.gameObjectNode.setFlagsRecursively(Flags.LOCKED);
            addChild(this.gameObjectNode);
        }
    }

    public String getGameObject() {
        return this.gameObject;
    }

    public void setGameObject(String gameObject) {
        this.gameObject = gameObject;
        reloadGameObject();
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.gameObjectNode == null) {
            reloadGameObject();
        }
    }

    public IStatus validateGameObject() {
        if (getModel() != null && !this.gameObject.isEmpty()) {
            if (this.gameObjectNode != null) {
                IStatus status = this.gameObjectNode.getStatus();
                if (!status.isOK()) {
                    return new Status(IStatus.ERROR, Constants.PLUGIN_ID, Messages.GameObjectInstanceNode_gameObject_INVALID_REFERENCE);
                }
            } else {
                int index = this.gameObject.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.GameObjectInstanceNode_gameObject_UNKNOWN_TYPE, this.gameObject);
                } else {
                    String ext = this.gameObject.substring(index+1);
                    message = NLS.bind(Messages.GameObjectInstanceNode_gameObject_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Constants.PLUGIN_ID, message);
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.gameObject);
    }

    @Override
    public boolean handleReload(IFile file) {
        IFile gameObjectFile = getModel().getFile(this.gameObject);
        if (gameObjectFile.exists() && gameObjectFile.equals(file)) {
            if (reloadGameObject()) {
                return true;
            }
        }
        return false;
    }

    private boolean reloadGameObject() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                removeChild(this.gameObjectNode);
                this.gameObjectNode = (GameObjectNode)model.loadNode(this.gameObject);
                if (this.gameObjectNode != null) {
                    this.gameObjectNode.setFlagsRecursively(Flags.LOCKED);
                    addChild(this.gameObjectNode);
                }
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
            return true;
        }
        return false;
    }

}
