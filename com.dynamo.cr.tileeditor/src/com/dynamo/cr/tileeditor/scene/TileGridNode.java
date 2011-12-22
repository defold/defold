package com.dynamo.cr.tileeditor.scene;

import java.nio.FloatBuffer;
import java.util.List;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.tile.TileSetUtil;

public class TileGridNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    @NotEmpty
    private String tileSet = "";

    private TileSetNode tileSetNode = null;

    // Graphics resources
    private FloatBuffer vertexData;

    @Override
    public void dispose() {
        super.dispose();
        if (this.tileSetNode != null) {
            this.tileSetNode.dispose();
        }
    }

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

    public void addLayer(LayerNode layer) {
        addChild(layer);
    }

    public TileSetNode getTileSetNode() {
        return this.tileSetNode;
    }

    public FloatBuffer getVertexData() {
        return this.vertexData;
    }

    @Override
    public void setFlags(Flags flag) {
        super.setFlags(flag);
        // Propagate locked flag to children
        // TODO Should this be handled in a more generic way (i.e. in Node)?
        // Not all flags should necessarily always be propagated, like TRANSFORMABLE.
        if (flag == Node.Flags.LOCKED) {
            List<Node> children = getChildren();
            for (Node child : children) {
                child.setFlags(flag);
            }
        }
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
            this.tileSetNode = null;
            if (!this.tileSet.isEmpty()) {
                try {
                    Node node = model.loadNode(this.tileSet);
                    if (node instanceof TileSetNode) {
                        this.tileSetNode = (TileSetNode)node;
                        this.tileSetNode.setModel(getModel());
                    }
                } catch (Exception e) {
                    // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
                }
            }
            updateVertexData();
            // attempted to reload
            return true;
        }
        return false;
    }

    private void updateVertexData() {
        if (this.tileSetNode == null || this.tileSetNode.getLoadedImage() == null || !validate().isOK() || !this.tileSetNode.validate().isOK()) {
            this.vertexData = null;
            return;
        }

        TileSetUtil.Metrics metrics = calculateMetrics(this.tileSetNode);
        if (metrics == null) {
            this.vertexData = null;
            return;
        }


    }

    private static TileSetUtil.Metrics calculateMetrics(TileSetNode node) {
        return TileSetUtil.calculateMetrics(node.getLoadedImage(), node.getTileWidth(), node.getTileHeight(), node.getTileMargin(), node.getTileSpacing(), null, 1.0f, 0.0f);
    }
}
