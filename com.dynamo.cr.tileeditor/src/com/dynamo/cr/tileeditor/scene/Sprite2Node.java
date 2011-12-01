package com.dynamo.cr.tileeditor.scene;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.tileeditor.core.TileSetModel;

public class Sprite2Node extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String tileSet = "";

    @Property
    private String defaultAnimation = "";

    private TileSetModel tileSetModel = null;

    public String getTileSet() {
        return tileSet;
    }

    public void setTileSet(String tileSet) {
        if (!this.tileSet.equals(tileSet)) {
            this.tileSet = tileSet;
            reloadTileSet();
            notifyChange();
        }
    }

    public String getDefaultAnimation() {
        return this.defaultAnimation;
    }

    public void setDefaultAnimation(String defaultAnimation) {
        if (!this.defaultAnimation.equals(defaultAnimation)) {
            this.defaultAnimation = defaultAnimation;
            notifyChange();
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.tileSetModel == null) {
            if (reloadTileSet()) {
                notifyChange();
            }
        }
    }

    @Override
    protected IStatus doValidate() {
        return validateProperties(new String[] {"tileSet", "defaultAnimation"});
    }

    @Override
    public void handleReload(IFile file) {
        IFile tileSetFile = getModel().getFile(this.tileSet);
        if (tileSetFile.exists() && tileSetFile.equals(file)) {
            if (reloadTileSet()) {
                notifyChange();
            }
        }
    }

    private boolean reloadTileSet() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                if (this.tileSetModel == null) {
                    this.tileSetModel = new TileSetModel(model.getContentRoot(), null, null, null);
                }
                IFile tileSetFile = model.getFile(this.tileSet);
                this.tileSetModel.load(tileSetFile.getContents());
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
                this.tileSetModel = null;
            }
            return true;
        }
        return false;
    }

}
