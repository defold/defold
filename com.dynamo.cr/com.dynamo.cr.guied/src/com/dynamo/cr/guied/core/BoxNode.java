package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;

@SuppressWarnings("serial")
public class BoxNode extends GuiNode {

    @Property(editorType = EditorType.DROP_DOWN)
    private String texture = "";

    @Property
    private Vector4d slice9 = new Vector4d(0,0,0,0);

    private transient TextureHandle textureHandle = new TextureHandle();

    public String getTexture() {
        return this.texture;
    }

    private void updateTexture() {
        if (!this.texture.isEmpty() && getModel() != null) {
            List<Node> textureNodes = getScene().getTexturesNode().getChildren();
            String texturePath = null;
            for (Node n : textureNodes) {
                TextureNode textureNode = (TextureNode) n;
                if (textureNode.getId().equals(this.texture)) {
                    texturePath = textureNode.getTexture();
                    break;
                }
            }
            if (texturePath != null) {
                if (this.textureHandle == null) {
                    this.textureHandle = new TextureHandle();
                }
                this.textureHandle.setImage(getModel().getImage(texturePath));
            }
        }
    }

    public void setTexture(String texture) {
        this.texture = texture;
        updateTexture();
    }

    public Object[] getTextureOptions() {
        List<Node> textureNodes = getScene().getTexturesNode().getChildren();
        List<String> textures = new ArrayList<String>(textureNodes.size());
        for (Node n : textureNodes) {
            textures.add(((TextureNode) n).getId());
        }
        return textures.toArray();
    }

    public TextureHandle getTextureHandle() {
        return this.textureHandle;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        updateTexture();
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        if (this.texture != null && !this.texture.isEmpty()) {
            IFile imgFile = getModel().getFile(this.texture);
            if (file.equals(imgFile)) {
                updateTexture();
                reloaded = true;
            }
        }
        return reloaded;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_IMAGE_ID);
    }

    public Vector4d getSlice9()
    {
        return new Vector4d(slice9);
    }

    public void setSlice9(Vector4d slice9)
    {
        this.slice9.set(slice9);
    }

}
