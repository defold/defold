package com.dynamo.cr.ddfeditor.scene;

import java.nio.FloatBuffer;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.model.proto.Model.ModelDesc.Builder;
import com.google.protobuf.Message;

@SuppressWarnings("serial")
public class ModelNode extends ComponentTypeNode {

    private transient MeshNode meshNode;

    @Property(editorType=EditorType.RESOURCE, extensions={"dae"})
    @Resource
    @NotEmpty
    private String mesh = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"material"})
    @Resource
    @NotEmpty
    private String material = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"png", "tga", "jpg", "jpeg", "gif", "bmp"})
    @Resource
    private String texture = "";

    public ModelNode(ModelDesc modelDesc) {
        super();
        setFlags(Flags.TRANSFORMABLE);
        mesh = modelDesc.getMesh();
        material = modelDesc.getMaterial();
        if (modelDesc.getTexturesCount() > 0) {
            texture = modelDesc.getTextures(0);
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
    public boolean handleReload(IFile file) {
        if (!this.mesh.isEmpty()) {
            IFile meshFile = getModel().getFile(this.mesh);
            if (meshFile.exists() && meshFile.equals(file)) {
                reload();
                return true;
            }
        }

        return false;
    }

    private boolean reload() {
        if (getModel() == null) {
            return false;
        }

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
    }

    public String getTexture() {
        return texture;
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
            .setMaterial(this.material);

        if (texture.length() > 0) {
            b.addTextures(texture);
        }
        return b.build();
    }

}
