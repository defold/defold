package com.dynamo.cr.guied.core;

import java.io.InputStream;
import java.io.InputStreamReader;

import javax.media.opengl.GL2;
import javax.vecmath.Vector3d;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class ParticleFXNode extends ClippingNode {

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String particlefx = "";
    
    private transient Particle.ParticleFX pfxDesc;

    public Vector3d getSize() {
        return new Vector3d(1.0, 1.0, 0.0);
    }

    public void setSize() {
        return;
    }

    public boolean isTextureVisible() {
        return false;
    }

    public boolean isAlphaVisible() {
        return true;
    }
    public boolean isInheritAlphaVisible() {
        return true;
    }

    public boolean isSizeVisible() {
        return false;
    }

    public boolean isColorVisible() {
        //return true;
        return false;
    }

    public boolean isBlendModeVisible() {
        //return true;
        return false;
    }

    public boolean isPivotVisible() {
        return true;
    }

    public boolean isAdjustModeVisible() {
        return true;
    }

    public boolean isXanchorVisible() {
        return true;
    }

    public boolean isYanchorVisible() {
        return true;
    }

    public boolean isSizeModeVisible() {
        return false;
    }

    // ParticleFX property
    public String getParticlefx() {
        return this.particlefx;
    }
    
    public void setParticlefx(String particleFX) {
        this.particlefx = particleFX;
        reloadResources();
        GuiNodeStateBuilder.setField(this, "particlefx", particleFX);
    }
    
    public void resetParticlefx() {
        this.particlefx = (String)GuiNodeStateBuilder.resetField(this, "particlefx");
        reloadResources();
    }
    
    public boolean isParticlefxOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "particlefx", this.particlefx);
    }
    
    public Object[] getParticlefxOptions() {
        ParticleFXScenesNode node = (ParticleFXScenesNode)getScene().getParticleFXScenesNode();
        return node.getParticleFXScenes(getModel()).toArray();
    }

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        /*if (this.textureSetNode != null) {
            this.textureSetNode.dispose(gl);
        }*/
    }
    
    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            reloadResources();
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        return reloadResources();
    }

    @Override
    public Image getIcon() {
        if(GuiNodeStateBuilder.isStateSet(this)) {
            if(isTemplateNodeChild()) {
                return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_TEMPLATE_IMAGE_ID);
            }
            return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_IMAGE_ID);
    }

    public boolean isSizeEditable() {
        return false;
    }

    private void updateAABB() {
        AABB aabb = new AABB();
        aabb.setIdentity();
        /*if (this.scene != null) {
            for (Mesh mesh : this.scene.meshes) {
                float[] v = mesh.vertices;
                int vertexCount = v.length / 5;
                for (int i = 0; i < vertexCount; ++i) {
                    int vi = i * 5;
                    aabb.union(v[vi+0], v[vi+1], v[vi+2]);
                }
            }
        }*/
        setAABB(aabb);
    }
    
    private ParticleFXSceneNode getParticleFXSceneNode() {
        ParticleFXSceneNode pfxSceneNode = ((ParticleFXScenesNode) getScene().getParticleFXScenesNode()).getParticleFXScenesNode(this.particlefx);
        if (pfxSceneNode != null) {
            TemplateNode parentTemplate = this.getParentTemplateNode();
            if (parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                pfxSceneNode = ((ParticleFXScenesNode) parentTemplate.getTemplateScene().getParticleFXScenesNode()).getParticleFXScenesNode(this.particlefx);
            }
        }
        return pfxSceneNode;
    }
    
    private static ParticleFX loadParticleFXDesc(ISceneModel model, String path) {
        if (!path.isEmpty()) {
            InputStream in = null;
            try {
                IFile file = model.getFile(path);
                in = file.getContents();
                Particle.ParticleFX.Builder builder = Particle.ParticleFX.newBuilder();
                TextFormat.merge(new InputStreamReader(in), builder);
                return builder.build();
            } catch (Exception e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            } finally {
                IOUtils.closeQuietly(in);
            }
        }
        return null;
    }

    private boolean reloadResources() {
        ISceneModel model = getModel();
        if (model != null && getParticleFXSceneNode() != null) {
            this.pfxDesc = loadParticleFXDesc(model, getParticleFXSceneNode().getParticlefx());
            
            updateAABB();
            updateStatus();
        }
        /*ISceneModel model = getModel();
        if (model != null && getSpineScenesNode() != null) {
            this.sceneDesc = loadSpineSceneDesc(model, getSpineScenesNode().getSpineScene());
            if (this.sceneDesc != null) {
                this.textureSetNode = loadTextureSetNode(model, this.sceneDesc.getAtlas());
                if (this.textureSetNode != null) {
                    this.scene = loadSpineScene(model, this.sceneDesc.getSpineJson(), new TransformProvider(this.textureSetNode.getRuntimeTextureSet()));
                }
            }
            updateAABB();
            updateStatus();
            // attempted to reload
            return true;
        } else {
            this.scene = null;
            this.sceneDesc = null;
            this.textureSetNode = null;
        }
        return false;*/
        
        return true;
    }
}