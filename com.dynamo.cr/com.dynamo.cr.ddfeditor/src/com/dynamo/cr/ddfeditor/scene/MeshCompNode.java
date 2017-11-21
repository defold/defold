package com.dynamo.cr.ddfeditor.scene;

import java.awt.image.BufferedImage;

import javax.media.opengl.GL2;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.mesh.proto.MeshProto.MeshDesc;
import com.dynamo.mesh.proto.MeshProto.MeshDesc.Builder;
import com.google.protobuf.Message;

@SuppressWarnings("serial")
public class MeshCompNode extends ComponentTypeNode {

    private transient TextureHandle textureHandle;

    @Property(editorType=EditorType.RESOURCE, extensions={"material"})
    @Resource
    @NotEmpty
    private String material = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"png", "tga", "jpg", "jpeg", "gif", "bmp", "cubemap"})
    @Resource
    private String texture = "";

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        textureHandle.clear(gl);
    }

    public TextureHandle getTextureHandle() {
        return textureHandle;
    }

    private void updateTexture() {
        if(this.textureHandle == null) {
            this.textureHandle = new TextureHandle();
        }
        if(this.texture.isEmpty()) {
            this.textureHandle.setImage(null);
            return;
        }
        BufferedImage image = getModel().getImage(this.texture);
        this.textureHandle.setImage(image);
    }


    public MeshCompNode(MeshDesc modelDesc) {
        super();
        setFlags(Flags.TRANSFORMABLE);
        material = modelDesc.getMaterial();
        if (modelDesc.getTexturesCount() > 0) {
            texture = modelDesc.getTextures(0);
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        //reload();
    }

    @Override
    protected IStatus validateNode() {
        // if (meshNode != null) {
        //     return meshNode.validateNode();
        // } else {
            return Status.OK_STATUS;
        // }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        // if (!this.mesh.isEmpty()) {
        //     updateTexture();

        //     IFile resFile = getModel().getFile(this.mesh);
        //     if (resFile.exists() && resFile.equals(file)) {
        //         reload();
        //         return true;
        //     }

        //     if(!this.skeleton.isEmpty()) {
        //         resFile = getModel().getFile(this.skeleton);
        //         if (resFile.exists() && resFile.equals(file)) {
        //             reload();
        //             return true;
        //         }
        //     }

        //     if(!this.animations.isEmpty()) {
        //         resFile = getModel().getFile(this.animations);
        //         if (resFile.exists() && resFile.equals(file)) {
        //             reload();
        //             return true;
        //         }
        //     }

        // }

        return false;
    }

    // private void updateAABB() {
    //     if (meshNode != null && meshNode.getPositions() != null) {
    //         AABB aabb = new AABB();

    //         FloatBuffer pos = meshNode.getPositions();
    //         for (int i = 0; i < pos.limit(); i+=3) {
    //             float x = pos.get(i+0);
    //             float y = pos.get(i+1);
    //             float z = pos.get(i+2);
    //             aabb.union(x, y, z);
    //         }
    //         setAABB(aabb);
    //     }
    // }

    public void setMaterial(String material) {
        this.material = material;
    }

    public String getMaterial() {
        return material;
    }

    public void setTexture(String texture) {
        this.texture = texture;
        updateTexture();
    }

    public String getTexture() {
        return texture;
    }

    public Message buildMessage() {
        Builder b = MeshDesc.newBuilder().setMaterial(this.material);

        if (texture.length() > 0) {
            b.addTextures(texture);
        }
        return b.build();
    }

}
