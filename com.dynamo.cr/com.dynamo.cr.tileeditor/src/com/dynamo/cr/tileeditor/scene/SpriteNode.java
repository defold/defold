package com.dynamo.cr.tileeditor.scene;

import javax.media.opengl.GL2;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.common.util.DDFUtil;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.gui.proto.Gui.NodeDesc.SizeMode;
import com.dynamo.sprite.proto.Sprite.SpriteDesc.BlendMode;

@SuppressWarnings("serial")
public class SpriteNode extends ComponentTypeNode {

    @Property(displayName="Image", editorType=EditorType.RESOURCE, extensions={"tileset", "tilesource","atlas"})
    @Resource
    @NotEmpty
    private String tileSource = "";

    @Property(editorType=EditorType.DROP_DOWN)
    @NotEmpty
    private String defaultAnimation = "";

    private transient TextureSetNode textureSetNode = null;

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    @Resource
    @NotEmpty
    private String material = "";

    @Property
    private BlendMode blendMode = BlendMode.BLEND_MODE_ALPHA;

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.textureSetNode != null) {
            this.textureSetNode.dispose(gl);
        }
    }

    public String getTileSource() {
        return tileSource;
    }

    public void setTileSource(String tileSource) {
        if (!this.tileSource.equals(tileSource)) {
            this.tileSource = tileSource;
            reloadTileSource();
        }
    }

    public IStatus validateTileSource() {
        if (this.textureSetNode != null) {
            this.textureSetNode.updateStatus();
            IStatus status = this.textureSetNode.getStatus();
            if (!status.isOK()) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.SpriteNode_tileSet_INVALID_REFERENCE);
            }
        } else if (!this.tileSource.isEmpty()) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.SpriteNode_tileSet_CONTENT_ERROR);
        }
        return Status.OK_STATUS;
    }

    public String getDefaultAnimation() {
        return this.defaultAnimation;
    }

    public void setDefaultAnimation(String defaultAnimation) {
        this.defaultAnimation = defaultAnimation;
        updateStatus();
        updateAABB();
    }

    public Object[] getDefaultAnimationOptions() {
        if (this.textureSetNode != null) {
            return this.textureSetNode.getAnimationIds().toArray();
        } else {
            return new Object[0];
        }
    }

    private void updateAABB() {
        if (this.textureSetNode != null) {
            AABB aabb = this.textureSetNode.getRuntimeTextureSet().getTextureBounds(this.defaultAnimation);
            setAABB(aabb);
        }
    }

    public IStatus validateDefaultAnimation() {
        if (!this.defaultAnimation.isEmpty()) {
            RuntimeTextureSet textureSet = null;
            if (this.textureSetNode != null) {
                textureSet = textureSetNode.getRuntimeTextureSet();
            }
            if (textureSet != null) {
                boolean exists = textureSet.getAnimation(this.defaultAnimation) != null;
                if (!exists) {
                    return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.SpriteNode_defaultAnimation_INVALID, this.defaultAnimation));
                }
            }
        }
        return Status.OK_STATUS;
    }

    public TextureSetNode getTextureSetNode() {
        return this.textureSetNode;
    }

    public String getMaterial() {
        return this.material;
    }

    public void setMaterial(String material) {
        this.material = material;
    }

    public BlendMode getBlendMode() {
        return this.blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
    }

    public IStatus validateBlendMode() {
        if (this.blendMode == BlendMode.BLEND_MODE_ADD_ALPHA) {
            String add = DDFUtil.getEnumValueDisplayName(BlendMode.BLEND_MODE_ADD.getValueDescriptor());
            String addAlpha = DDFUtil.getEnumValueDisplayName(BlendMode.BLEND_MODE_ADD_ALPHA.getValueDescriptor());
            return new Status(Status.WARNING, Activator.PLUGIN_ID, String.format("'%s' has been replaced by '%s'",
                    addAlpha, add));
        } else {
            return Status.OK_STATUS;
        }
    }

    @Override
    public void parentSet() {
        if (getParent() != null) {
            setFlags(Flags.TRANSFORMABLE);
        } else {
            clearFlags(Flags.TRANSFORMABLE);
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.textureSetNode == null) {
            reloadTileSource();
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        if (!this.tileSource.isEmpty()) {
            IFile tileSetFile = getModel().getFile(this.tileSource);
            if (tileSetFile.exists() && tileSetFile.equals(file)) {
                if (reloadTileSource()) {
                    reloaded = true;
                }
            }
            if (this.textureSetNode != null) {
                if (this.textureSetNode.handleReload(file, childWasReloaded)) {
                    reloaded = true;
                }
            }
        }
        return reloaded;
    }

    private boolean reloadTileSource() {
        ISceneModel model = getModel();
        if (model != null) {
            this.textureSetNode = null;
            if (!this.tileSource.isEmpty()) {
                try {
                    Node node = model.loadNode(this.tileSource);
                    if (node instanceof TextureSetNode) {
                        this.textureSetNode = (TextureSetNode)node;
                        this.textureSetNode.setModel(getModel());
                        updateStatus();
                    }
                } catch (Exception e) {
                    // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
                }
            }
            updateAABB();
            // attempted to reload
            return true;
        }
        return false;
    }

    @Property(category ="Textures")
    private String texture0 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture1 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture2 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture3 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture4 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture5 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture6 = "";

    @Property(category ="Textures", editorType=EditorType.RESOURCE, extensions={"tilesource","atlas", "png", "jpg" })
    @Resource
    private String texture7 = "";

    public boolean isTexture0Editable() {
        return false;
    }

    public String getTexture0() {
        return texture0 = getTileSource();
    }

    public void setTexture0(String s) {
        texture0 = getTileSource();
    }

    public String getTexture1() {
        return texture1;
    }
    public void setTexture1(String s) {
        texture1 = s;
    }

    public String getTexture2() {
        return texture2;
    }

    public void setTexture2(String s) {
        texture2 = s;
    }

    public String getTexture3() {
        return texture3;
    }

    public void setTexture3(String s) {
        texture3 = s;
    }

    public String getTexture4() {
        return texture4;
    }

    public void setTexture4(String s) {
        texture4 = s;
    }

    public String getTexture5() {
        return texture5;
    }

    public void setTexture5(String s) {
        texture5 = s;
    }

    public String getTexture6() {
        return texture6;
    }

    public void setTexture6(String s) {
        texture6 = s;
    }

    public String getTexture7() {
        return texture7;
    }

    public void setTexture7(String s) {
        texture7 = s;
    }

}
