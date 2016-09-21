package com.dynamo.cr.ddfeditor.scene;

import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.util.ArrayList;

import javax.media.opengl.GL2;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.bob.pipeline.ColladaUtil;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.model.proto.Model.ModelDesc.Builder;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.google.protobuf.Message;

@SuppressWarnings("serial")
public class ModelNode extends ComponentTypeNode {

    private transient MeshNode meshNode;
    private transient TextureHandle textureHandle = new TextureHandle();
    private transient Skeleton rigSkeleton;

    @Property(editorType=EditorType.RESOURCE, extensions={"dae"})
    @Resource
    @NotEmpty
    private String mesh = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"material"})
    @Resource
    @NotEmpty
    private String material = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"png", "tga", "jpg", "jpeg", "gif", "bmp", "cubemap"})
    @Resource
    private String texture = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"dae"})
    @Resource
    private String skeleton = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"dae"})
    @Resource
    private String animations = "";


    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        textureHandle.clear(gl);
    }

    public TextureHandle getTextureHandle() {
        return textureHandle;
    }

    private void updateTexture() {
        BufferedImage image = getModel().getImage(this.texture);
        this.textureHandle.setImage(image);
    }


    public ModelNode(ModelDesc modelDesc) {
        super();
        setFlags(Flags.TRANSFORMABLE);
        mesh = modelDesc.getMesh();
        material = modelDesc.getMaterial();
        if (modelDesc.getTexturesCount() > 0) {
            texture = modelDesc.getTextures(0);
        }
        skeleton = modelDesc.getSkeleton();
        if (modelDesc.getAnimationsCount() > 0) {
            animations = modelDesc.getAnimations(0);
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        reload();
    }

    @Override
    protected IStatus validateNode() {
        if (meshNode != null) {
            return meshNode.validateNode();
        } else {
            return Status.OK_STATUS;
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        if (!this.mesh.isEmpty()) {
            updateTexture();

            IFile meshFile = getModel().getFile(this.mesh);
            if (meshFile.exists() && meshFile.equals(file)) {
                reload();
                return true;
            }
        }

        return false;
    }

    private void updateBones() {
        rigSkeleton = Skeleton.newBuilder().build();
        for(Node child : getChildren()) {
            removeChild(child);
        }
        if(skeleton.isEmpty()) {
            updateStatus();
            return;
        }


        Skeleton.Builder b = Skeleton.newBuilder();
        ArrayList<String> boneIds = new ArrayList<String>();
        try {
            IFile skeletonFile = getModel().getFile(this.skeleton);
            ColladaUtil.loadSkeleton(skeletonFile.getContents(), b, boneIds);
        } catch (Exception e) {
            updateStatus();
            return;
        }

/*        for(Bone bone : bones) {
            ModelBoneNode n = new ModelBoneNode(bone.toString());
            // n.setFlags(Flags.LOCKED);
            addChild(n);
        }
*/
        rigSkeleton = b.build();
        updateStatus();
    }

    private boolean reload() {
        if (getModel() == null) {
            return false;
        }
        updateTexture();
        updateBones();

        if (!mesh.equals("")) {
            try {
                meshNode = (MeshNode) getModel().loadNode(mesh);
                updateAABB();
                return true;
            } catch (Exception e) {
            }
        }

        return false;
    }

    private void updateAABB() {
        if (meshNode != null && meshNode.getPositions() != null) {
            AABB aabb = new AABB();

            FloatBuffer pos = meshNode.getPositions();
            for (int i = 0; i < pos.limit(); i+=3) {
                float x = pos.get(i+0);
                float y = pos.get(i+1);
                float z = pos.get(i+2);
                aabb.union(x, y, z);
            }
            setAABB(aabb);
        }
    }

    public void setMesh(String mesh) {
        this.mesh = mesh;
        reload();
    }

    public String getMesh() {
        return mesh;
    }

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

    public void setSkeleton(String mesh) {
        this.skeleton = mesh;
        reload();
    }

    public String getSkeleton() {
        return skeleton;
    }

    public void setAnimations(String mesh) {
        this.animations = mesh;
        reload();
    }

    public String getAnimations() {
        return animations;
    }

    public MeshNode getMeshNode() {
        return meshNode;
    }

    public void setMeshNode(MeshNode meshNode) {
        this.meshNode = meshNode;
    }

    public Message buildMessage() {
        Builder b = ModelDesc.newBuilder()
            .setMesh(this.mesh)
            .setMaterial(this.material)
            .setSkeleton(this.skeleton);

        if (texture.length() > 0) {
            b.addTextures(texture);
        }
        if (animations.length() > 0) {
            b.addAnimations(animations);
        }

        return b.build();
    }

}
