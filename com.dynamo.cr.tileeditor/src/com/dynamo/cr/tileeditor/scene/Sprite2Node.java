package com.dynamo.cr.tileeditor.scene;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;

public class Sprite2Node extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    @NotEmpty
    private String tileSet = "";

    @Property
    @NotEmpty
    private String defaultAnimation = "";

    private TileSetNode tileSetNode = null;

    public String getTileSet() {
        return tileSet;
    }

    public void setTileSet(String tileSet) {
        if (!this.tileSet.equals(tileSet)) {
            this.tileSet = tileSet;
            reloadTileSet();
        }
    }

    public IStatus validateTileSet() {
        if (this.tileSetNode != null) {
            IStatus status = this.tileSetNode.validate();
            if (!status.isOK()) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.Sprite2Node_tileSet_INVALID_REFERENCE);
            }
        } else if (!this.tileSet.isEmpty()) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.Sprite2Node_tileSet_INVALID_TYPE);
        }
        return Status.OK_STATUS;
    }

    public String getDefaultAnimation() {
        return this.defaultAnimation;
    }

    public void setDefaultAnimation(String defaultAnimation) {
        this.defaultAnimation = defaultAnimation;
    }

    public IStatus validateDefaultAnimation() {
        if (!this.defaultAnimation.isEmpty()) {
            if (this.tileSetNode != null) {
                boolean exists = false;
                for (AnimationNode animation : this.tileSetNode.getAnimations()) {
                    if (animation.getId().equals(this.defaultAnimation)) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.Sprite2Node_defaultAnimation_INVALID, this.defaultAnimation));
                }
            }
        }
        return Status.OK_STATUS;
    }

    public TileSetNode getTileSetNode() {
        return this.tileSetNode;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.tileSetNode == null) {
            reloadTileSet();
        }
    }

    @Override
    public boolean handleReload(IFile file) {
        boolean reloaded = false;
        IFile tileSetFile = getModel().getFile(this.tileSet);
        if (tileSetFile.exists() && tileSetFile.equals(file)) {
            if (reloadTileSet()) {
                reloaded = true;
            }
        }
        if (this.tileSetNode != null) {
            if (this.tileSetNode.handleReload(file)) {
                reloaded = true;
            }
        }
        return reloaded;
    }

    private boolean reloadTileSet() {
        ISceneModel model = getModel();
        if (model != null) {
            if (!this.tileSet.isEmpty()) {
                this.tileSetNode = null;
                try {
                    Node node = model.loadNode(this.tileSet);
                    if (node instanceof TileSetNode) {
                        this.tileSetNode = (TileSetNode)node;
                        this.tileSetNode.setModel(getModel());
                    }
                } catch (Exception e) {
                    // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
                }
                // attempted to reload
                return true;
            }
        }
        return false;
    }

}
