package com.dynamo.cr.tileeditor.scene;

import java.nio.FloatBuffer;
import java.util.List;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.tile.TileSetUtil;

public class Sprite2Node extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    @NotEmpty
    private String tileSet = "";

    @Property
    @NotEmpty
    private String defaultAnimation = "";

    private TileSetNode tileSetNode = null;

    // Graphics resources
    private FloatBuffer vertexData;

    public Sprite2Node() {
        setFlags(Flags.TRANSFORMABLE);
    }

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

    public String getDefaultAnimation() {
        return this.defaultAnimation;
    }

    public void setDefaultAnimation(String defaultAnimation) {
        this.defaultAnimation = defaultAnimation;
        updateVertexData();
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

    public FloatBuffer getVertexData() {
        return this.vertexData;
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
        if (!this.tileSet.isEmpty()) {
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

        int tile = 0;
        List<AnimationNode> animations = this.tileSetNode.getAnimations();
        for (AnimationNode animation : animations) {
            if (animation.getId().equals(this.defaultAnimation)) {
                tile = animation.getStartTile() - 1;
                break;
            }
        }

        float recipImageWidth = 1.0f / metrics.tileSetWidth;
        float recipImageHeight = 1.0f / metrics.tileSetHeight;

        int tileWidth = this.tileSetNode.getTileWidth();
        int tileHeight = this.tileSetNode.getTileHeight();

        float f = 0.5f;
        float x0 = -f * tileWidth;
        float x1 = f * tileWidth;
        float y0 = -f * tileHeight;
        float y1 = f * tileHeight;

        int tileMargin = this.tileSetNode.getTileMargin();
        int tileSpacing = this.tileSetNode.getTileSpacing();
        int x = tile % metrics.tilesPerRow;
        int y = tile / metrics.tilesPerRow;
        float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
        float u1 = u0 + tileWidth * recipImageWidth;
        float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
        float v1 = v0 + tileHeight * recipImageHeight;

        final int vertexCount = 4;
        final int componentCount = 5;
        this.vertexData = FloatBuffer.allocate(vertexCount * componentCount);
        FloatBuffer v = this.vertexData;
        float z = 0.0f;
        v.put(u0); v.put(v1); v.put(x0); v.put(y0); v.put(z);
        v.put(u0); v.put(v0); v.put(x0); v.put(y1); v.put(z);
        v.put(u1); v.put(v0); v.put(x1); v.put(y1); v.put(z);
        v.put(u1); v.put(v1); v.put(x1); v.put(y0); v.put(z);
        v.flip();

        AABB aabb = new AABB();
        aabb.union(x0, y0, z);
        aabb.union(x1, y1, z);
        setAABB(aabb);
    }

    private static TileSetUtil.Metrics calculateMetrics(TileSetNode node) {
        return TileSetUtil.calculateMetrics(node.getLoadedImage(), node.getTileWidth(), node.getTileHeight(), node.getTileMargin(), node.getTileSpacing(), null, 1.0f, 0.0f);
    }

}
